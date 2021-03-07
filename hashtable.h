#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stdlib.h>
#include <string.h>

struct hashtable;

typedef void (*ht_enum_func)(const void *key, const size_t key_length, const void *value, const size_t value_length, const void *obj);

extern struct hashtable *ht_new(size_t capacity);
extern void ht_delete(struct hashtable *map);
extern int ht_get(const struct hashtable *map, const void *key, const size_t key_length, void *out_buf, const size_t n_out_buf);
extern int ht_exists(const struct hashtable *map, const void *key, const size_t key_length);
extern int ht_put(struct hashtable *map, void *key, size_t key_size, void *value, size_t value_size);
extern int ht_get_count(const struct hashtable *map);
extern int ht_enum(const struct hashtable *map, ht_enum_func enum_func, const size_t key_size, const size_t value_size, const void *obj);

#endif // HASHTABLE_H

