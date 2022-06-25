/* this file has been modified again by Filip Pizlo, 2003, 2004, 2005, 2015, to be
 * incorporated into tsf. */
/* this file has been modified by Filip Pizlo, 2002, to support C++ */
/* This is a public domain general purpose hash table package written by Peter Moore @ UCB. */

/* @(#) st.h 5.1 89/12/14 */

#ifndef TSF_ST_INCLUDED

#define TSF_ST_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tsf_st_table tsf_st_table;
typedef struct tsf_st_iterator tsf_st_iterator;

struct tsf_st_hash_type {
    int (*compare)();
    int (*hash)();
};

typedef struct tsf_st_table_entry tsf_st_table_entry;

struct tsf_st_table_entry {
    unsigned int hash;
    char *key;
    void *record;
    tsf_st_table_entry *next;
};

struct tsf_st_table {
    struct tsf_st_hash_type *type;
    int num_bins;
    int num_entries;
    tsf_st_table_entry **bins;
};

struct tsf_st_iterator {
    tsf_st_table *table;
    int i;
    struct tsf_st_table_entry *ptr;
};

#define tsf_st_is_member(table,key) tsf_st_lookup(table,key,(char **)0)

enum tsf_st_retval {TSF_ST_CONTINUE, TSF_ST_STOP, TSF_ST_DELETE};

typedef enum tsf_st_retval (*tsf_st_func_t)(void*,void*,void*);

tsf_st_table *tsf_st_init_table(struct tsf_st_hash_type *type);
tsf_st_table *tsf_st_init_table_with_size(struct tsf_st_hash_type *type,int size);
tsf_st_table *tsf_st_init_numtable();
tsf_st_table *tsf_st_init_numtable_with_size(int size);
tsf_st_table *tsf_st_init_ptrtable();
tsf_st_table *tsf_st_init_ptrtable_with_size(int size);
tsf_st_table *tsf_st_init_strtable();
tsf_st_table *tsf_st_init_strtable_with_size(int size);
int     tsf_st_delete(tsf_st_table *table,char **key,void **value),
    tsf_st_delete_safe(tsf_st_table *table,char **key,
        void **value,char *never);
int     tsf_st_insert(tsf_st_table *table,char *key,void *value),
    tsf_st_insert_noreplace(tsf_st_table *table,char *key,void *value),
    tsf_st_add_direct(tsf_st_table *table,char *key,void *value),
    tsf_st_lookup(tsf_st_table *table,char *key,void **value);
void    tsf_st_foreach(tsf_st_table *table,tsf_st_func_t func,void *arg),
    tsf_st_free_table(tsf_st_table *table),
    tsf_st_cleanup_safe(tsf_st_table *table,char *never);
tsf_st_table *tsf_st_copy(tsf_st_table *old_table);
void    tsf_st_iterator_init(tsf_st_table *table,
                             tsf_st_iterator *iter),
        tsf_st_iterator_get(tsf_st_iterator *iter,
                            char **key,
                            void **value),
        tsf_st_iterator_next(tsf_st_iterator *iter);
int     tsf_st_iterator_valid(tsf_st_iterator *iter);

int tsf_st_strhash();

static TSF_inline int tsf_st_ptrhash(void* x) {
    intptr_t value = (intptr_t)x;
    value >>= sizeof(void*);
    value += value >> 13;
    if (sizeof(void*) > 4)
        value += (value >> 32) * 7;
    return (int)value;
}

/* fast lookup for pointers. */
static TSF_inline
int tsf_st_lookup_ptrtable(tsf_st_table *table, void *key, void **value) {
    unsigned int hash_val, bin_pos;
    tsf_st_table_entry *ptr;

    hash_val = tsf_st_ptrhash(key);
    bin_pos = hash_val&(table)->num_bins;
    ptr = (table)->bins[bin_pos];
    if (ptr && key != ptr->key) {
        while (ptr->next && key != ptr->next->key) {
            ptr = ptr->next;
        }
        ptr = ptr->next;
    }

    if (ptr == 0) {
        return 0;
    } else {
        if (value != 0)  *value = ptr->record;
        return 1;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* TSF_ST_INCLUDED */
