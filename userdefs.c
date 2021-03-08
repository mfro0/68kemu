#include "userdefs.h"
#include "hashtable.h"

#include <stdbool.h>
#include <stdio.h>
#include <gem.h>
#include <mint/osbind.h>

#include "m68ksubr.h"
#include "debug.h"

typedef short (*ufunc)(PARMBLK *pb);

struct udh_hashobj
{
    OBJECT *tree;
    short obj;
};

struct udf_hashobj
{
    USERBLK *ub;
};

static struct hashtable *userdef_ht = NULL;
static struct hashtable *functions_ht = NULL;

#define STACKSIZE   4 * 1024 / sizeof(long)
static long our_stack[STACKSIZE];    /* 4k of stack for the callback */

short do_ud(PARMBLK *pb)
{
    dbg("pb = %p\r\n", pb);

    /*
     * lookup original userdef function by tree and object extracted from
     * parameter block in our hash table
     */
    struct udh_hashobj h = { .tree = pb->pb_tree, .obj = pb->pb_obj };

    ufunc f = NULL;

    if (ht_get(userdef_ht, &h, sizeof(h), &f, sizeof(f)))
    {
        dbg("userdef found in hashtable (tree=0x%x, obj=%d, f=%p)\r\n",
            pb->pb_tree, pb->pb_obj, f);
        dbg("calling callback at %p\r\n", f);
    }
    else
    {
        dbg("userdef not found in hashtable\r\n");
    }

    /*
     * exec f with pb as parameter in the emulator
     */

    long stack[STACKSIZE];       /* 4k of stack for the emulated subroutine */

    dbg("pb at %p\r\n", pb);
    dbg("x=%d, y=%d, w=%d, h=%d\r\n", pb->pb_x, pb->pb_y, pb->pb_w, pb->pb_h);
    dbg("obj=%d, parm=%lx, tree=%p, currstate=%d, prevstate=%d\r\n",
        pb->pb_obj, pb->pb_parm, pb->pb_tree, pb->pb_currstate, pb->pb_prevstate);
    stack[STACKSIZE - 1] = (long) pb;
    stack[STACKSIZE - 2] = RETURN_STACK_MARKER;
    m68k_execute_subroutine((long) &stack[STACKSIZE - 2], (long) f);

    return 0;
}

/*
 * put the callback function f into a hashtable with a key generated
 * of tree and object of the corresponding USERDEF object for
 * later retrieval (when the real callback arrives from the AES)
 */
int insert_ud(ufunc f, OBJECT *tree, short obj)
{
    struct udh_hashobj h = { .tree = tree, .obj = obj };

    /*
     * allocate hash table if it doesn't exist yet
     */
    if (userdef_ht == NULL)
        userdef_ht = ht_new(40);

    /*
     * insert our newly created hash object into the hash table
     */
    if (userdef_ht != NULL && !ht_exists(userdef_ht, &h, sizeof(h)))
    {
        dbg("insert tree 0x%x obj %d into hashtable, function %p\r\n",
            tree, obj, f);
        return ht_put(userdef_ht, &h, sizeof(h), &f, sizeof(f));
    }
    else
        dbg("apparently tree 0x%x obj %d is already there\r\n", tree, obj);

    return 0;
}

struct uf_value
{
    ufunc f;
    ufunc ori_f;
};

/*
 * build up a hashtable with a key of the USERBLK address and values
 * of the assigned original and intercepted callback function. We need to
 * do that since many applications reuse their USERBLKs for more than one single object.
 */
int insert_uf(USERBLK *ub, ufunc f, ufunc ori_f)
{
    int res = 0;
    struct uf_value v = { .f = f, .ori_f = ori_f };

    /*
     * allocate hash table if it doesn't exist yet
     */
    if (functions_ht == NULL)
        functions_ht = ht_new(40);

    /*
     * insert our newly created hash object into the hash table
     */

    if (functions_ht != NULL && !ht_exists(functions_ht, &ub, sizeof(USERBLK *)))
    {
        dbg("insert function %p, original function %p for userblk %p\r\n",
            f, ori_f, ub);
        res = ht_put(functions_ht, &ub, sizeof(USERBLK *), &v, sizeof(v));
    }
    /*
     * do a test retrieval of our hashtable put we just did
     */

    int r;

    r = ht_get(functions_ht, &ub, sizeof(USERBLK *), &v, sizeof(v));
    if (r != 0)
    {
        dbg("v.f = %p, v.ori_f = %p\r\n", v.f, v.ori_f);
    }
    else
        dbg("ht_get failed\r\n");

    return res;
}

static void fixit(OBJECT *tree, short obj)
{
    /*
     * be sure to mask out the upper 8 bits of ob_type since these probably bring
     * additional USERDEF flags and are generally ignored by the AES
     */
    if ((tree[obj].ob_type & 0x00ff) == G_USERDEF)
    {
        USERBLK *ub = (USERBLK *) tree[obj].ob_spec.userblk;

        dbg("userdef found, tree %p obj %d func address=%p\r\n",
            tree, obj, ub->ub_code);
        /*
         * caveat: possible reuse of user blk
         * If the userdef function reuses a USERBLK object we already changed, we would insert
         * our hook function as original function, effectvely producing an infinite loop by having this
         * function call itself on callback. To avoid this, we search through the hash table to see if we
         * had this already and take the original address stored there.
         */
        if (ub->ub_code != do_ud)       /* caveat: possible reuse of user blk! */
        {
            if (!insert_ud((ufunc) ub->ub_code, tree, obj))
                dbg("insert_ud() failed\r\n");

            if (!insert_uf(ub,  do_ud, (ufunc) ub->ub_code))
                dbg("insert_uf() failed\r\n");

            ub->ub_code = do_ud;
        }
        else
        {
            /*
             * as our intermediate function is already in the userblk, insert the original one into
             * the function replacement hashtable
             */
            struct uf_value v;
            if (ht_get(functions_ht, &ub, sizeof(USERBLK *), &v, sizeof(v)))
            {
                insert_ud((ufunc) v.ori_f, tree, obj);
            }
            else
            {
                dbg("function for USERBLK %p not found\r\n", ub);
            }
        }
    }
}

/*
 * recursively walk a complete object tree and call callrout() for every object encountered
 */
static void tree_walk(OBJECT *tree, short start, void (*callrout)(OBJECT *tree, short obj))
{
    short i;

    for (i = tree[start].ob_head; (i != start) && (i != NIL); i = tree[i].ob_next)
    {
        (*callrout)(tree, i);
        tree_walk(tree, i, callrout);
    }
}

/*
 * traverse the object tree beginning with obj until max_depth depth
 * and search for objects of G_USERDEF type. If we find one, replace
 * the callback address with our own one (so we can execute the userdef
 * emulated and insert the original address into our hash table to be able
 * to prepare a proper emulator environment for it
 *
 * the max_depth parameter is ignored for now
 */
void fix_userdefs(OBJECT *tree, short obj, short max_depth)
{
    //dbg("fixing userdefs of tree %p obj %d with depth %d\r\n", tree, obj, max_depth);
    tree_walk(tree, obj, fixit);
}

void cleanup_userdefs(void)
{
    /*
     * cleanup userdefs and release any memory we were using
     */
    if (userdef_ht != NULL)
        ht_delete(userdef_ht);
}
