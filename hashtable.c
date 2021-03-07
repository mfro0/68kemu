#include "hashtable.h"

#include <stdlib.h>
#include <string.h>

struct kv
{
    void *key;
    void *value;
};

struct bucket
{
    size_t count;
    struct kv *kvs;
};

struct hashtable
{
    size_t count;
    struct bucket *buckets;
};

static struct kv *get_kv(struct bucket *bucket, const void *key, const size_t key_len);
static unsigned long hash(const void *value, const size_t value_length);

struct hashtable *ht_new(size_t capacity)
{
    struct hashtable *map;

    map = malloc(sizeof (struct hashtable));
    if (map == NULL)
        return NULL;

    map->count = capacity;
    map->buckets = malloc(map->count * sizeof(struct bucket));
    if (map->buckets == NULL)
    {
        free(map);
        return NULL;
    }

    memset(map->buckets, 0, map->count * sizeof(struct bucket));

    return map;
}

void ht_delete(struct hashtable *map)
{
    size_t i;
    size_t j;
    size_t n;
    size_t m;
    struct bucket *bucket;
    struct kv *pair;

    if (map == NULL)
        return;

    n = map->count;
    bucket = map->buckets;
    i = 0;

    while (i < n)
    {
        m = bucket->count;
        pair = bucket->kvs;
        j = 0;
        while (j < m)
        {
            free(pair->key);
            free(pair->value);
            pair++;
            j++;
        }
        free(bucket->kvs);
        bucket++;
        i++;
    }
    free(map->buckets);
    free(map);
}

int ht_get(const struct hashtable *map, const void *key, const size_t key_length, void *out_buf, const size_t n_out_buf)
{
    size_t index;
    struct bucket *bucket;
    struct kv *pair;

    if (map == NULL)
        return 0;

    if (key == NULL)
        return 0;

    index = hash(key, key_length) % map->count;
    bucket = &(map->buckets[index]);
    pair = get_kv(bucket, key, key_length);

    if (pair == NULL)
        return 0;

    if (out_buf == NULL)
        return 0;

    memcpy(out_buf, pair->value, n_out_buf);

    return 1;
}


int ht_exists(const struct hashtable *map, const void *key, const size_t key_length)
{
    size_t index;
    struct bucket *bucket;
    struct kv *pair;

    if (map == NULL)
        return 0;

    if (key == NULL)
        return 0;

    index = hash(key, key_length) % map->count;
    bucket = &(map->buckets[index]);
    pair = get_kv(bucket, key, key_length);
    if (pair == NULL)
        return 0;

    return 1;
}

int ht_put(struct hashtable *map, void *key, size_t key_size, void *value, size_t value_size)
{
    size_t index;
    struct bucket *bucket;
    struct kv *tmp_pairs;
    struct kv *pair;
    char *tmp_value;
    char *new_key;
    char *new_value;

    if (map == NULL)
        return 0;

    if (key == NULL || value == NULL)
        return 0;

    /*
     * get a pointer to the bucket the key string hashes to
     */
    index = hash(key, key_size) % map->count;
    bucket = &(map->buckets[index]);

    /*
     * check if we can handle insertion by simply replacing an existing key-value pair in the bucket
     */
    if ((pair = get_kv(bucket, key, key_size)) != NULL)
    {
        /*
         * the bucket contains a pair that matches the provided key,
         * change the value for that pair to the new value
         */

        tmp_value = realloc(pair->value, value_size);
        if (tmp_value == NULL)
            return 0;

        pair->value = tmp_value;

        /*
         * copy the new value into the pair that matches the key
         */
        memcpy(pair->value, value, value_size);

        return 1;
    }

    /*
     * allocate space for the new key and value
     */
    new_key = malloc(key_size);
    if (new_key == NULL)
        return 0;

    new_value = malloc(value_size);
    if (new_value == NULL)
    {
        free(new_key);
        return 0;
    }

    /*
     * create a key-value pair
     */
    if (bucket->count == 0)
    {
        bucket->kvs = malloc(sizeof(struct kv));
        if (bucket->kvs == NULL)
        {
            free(new_key);
            free(new_value);
            return 0;
        }
        bucket->count = 1;
    }
    else
    {
        tmp_pairs = realloc(bucket->kvs, (bucket->count + 1) * sizeof (struct kv));
        if (tmp_pairs == NULL)
        {
            free(new_key);
            free(new_value);
            return 0;
        }
        bucket->kvs = tmp_pairs;
        bucket->count++;
    }
    pair = &(bucket->kvs[bucket->count - 1]);
    pair->key = new_key;
    pair->value = new_value;
    memcpy(pair->key, key, key_size);
    memcpy(pair->value, value, value_size);

    return 1;
}

int ht_get_count(const struct hashtable *map)
{
    size_t i;
    size_t j;
    size_t n;
    size_t m;
    size_t count;
    struct bucket *bucket;
    struct kv *pair;

    if (map == NULL)
        return 0;

    bucket = map->buckets;
    n = map->count;
    i = 0;
    count = 0;

    while (i < n)
    {
        pair = bucket->kvs;
        m = bucket->count;
        j = 0;
        while (j < m)
        {
            count++;
            pair++;
            j++;
        }
        bucket++;
        i++;
    }
    return count;
}

int ht_enum(const struct hashtable *map, ht_enum_func enum_func, const size_t key_size, const size_t value_size, const void *obj)
{
    size_t i;
    size_t j;
    size_t n;
    size_t m;
    struct bucket *bucket;
    struct kv *pair;

    if (map == NULL)
        return 0;

    if (enum_func == NULL)
        return 0;

    bucket = map->buckets;
    n = map->count;
    i = 0;

    while (i < n)
    {
        pair = bucket->kvs;
        m = bucket->count;
        j = 0;
        while (j < m)
        {
            enum_func(pair->key, key_size, pair->value, value_size, obj);
            pair++;
            j++;
        }
        bucket++;
        i++;
    }
    return 1;
}

static struct kv *get_kv(struct bucket *bucket, const void *key, const size_t key_len)
{
    size_t i;
    size_t n;
    struct kv *pair;

    n = bucket->count;
    if (n == 0)
        return NULL;

    pair = bucket->kvs;
    i = 0;

    while (i < n)
    {
        if (pair->key != NULL && pair->value != NULL)
        {
            if (memcmp(pair->key, key, key_len) == 0)
                return pair;
        }
        pair++;
        i++;
    }
    return NULL;
}

static unsigned long hash(const void *value, const size_t value_length)
{
    unsigned long hash = 5381;
    int c;
    int i;

    for (i = 0; i < value_length; i++)
    {
        c = ((char *) value)[i];
        hash = ((hash << 5) + hash) + c;
    }

    return hash;
}
