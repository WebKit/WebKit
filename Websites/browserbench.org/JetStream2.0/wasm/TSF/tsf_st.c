/* this file has been modified again by Filip Pizlo, 2003, 2004, 2005, to be
 * incorporated into tsf. */
/* this file has been modified by Filip Pizlo, 2002, to support C++ */
/* This is a public domain general purpose hash table package written by Peter Moore @ UCB. */

/* FIXME: make this code check for out-of-memory errors and do the
 * right thing, which should include using tsf_set_error() and friends. */

#include "tsf_internal.h"
#include <stdio.h>

#ifdef NT
#include <malloc.h>
#endif

#define TSF_ST_DEFAULT_MAX_DENSITY 5
#define TSF_ST_DEFAULT_INIT_TABLE_SIZE 11

    /*
     * DEFAULT_MAX_DENSITY is the default for the largest we allow the
     * average number of items per bin before increasing the number of
     * bins
     *
     * DEFAULT_INIT_TABLE_SIZE is the default for the number of bins
     * allocated initially
     *
     */

static int numcmp();
static int numhash();
static struct tsf_st_hash_type type_numhash = {
    numcmp,
    numhash,
};

static int ptrcmp();
static struct tsf_st_hash_type type_ptrhash = {
    ptrcmp,
    tsf_st_ptrhash,
};

#undef strcmp
extern int strcmp();
int tsf_st_strhash();
static struct tsf_st_hash_type type_strhash = {
    strcmp,
    tsf_st_strhash,
};

#include <stdlib.h>
#define xmalloc malloc
#define xcalloc calloc
#define xrealloc realloc
#define xfree free

static int rehash();

#define alloc(type) (type*)xmalloc((unsigned)sizeof(type))
#define Calloc(n,s) (char*)xcalloc((n),(s))

#define EQUAL(table,x,y) ((x)==(y) || (*table->type->compare)((x),(y)) == 0)

#define do_hash(key,table) (unsigned int)(*(table)->type->hash)((key))
#define do_hash_bin(key,table) (do_hash(key, table)&(table)->num_bins)

/*
 * MINSIZE is the minimum size of a dictionary.
 */

#define MINSIZE 8

static int
new_size(size)
    int size;
{
    int i;

    for (i=3; i<31; i++) {
        if ((1<<i) > size) return 1<<i;
    }
    return -1;
}

#ifdef HASH_LOG
static int collision = 0;
static int init_st = 0;

static void
stat_col()
{
    FILE *f = fopen("/tmp/col", "w");
    fprintf(f, "collision: %d\n", collision);
    fclose(f);
}
#endif

tsf_st_table*
tsf_st_init_table_with_size(type, size)
    struct tsf_st_hash_type *type;
    int size;
{
    tsf_st_table *tbl;

#ifdef HASH_LOG
    if (init_st == 0) {
        init_st = 1;
        atexit(stat_col);
    }
#endif

    size = new_size(size);      /* round up to prime number */

    tbl = alloc(tsf_st_table);
    if (tbl==NULL) {
        return NULL;
    }
    tbl->type = type;
    tbl->num_entries = 0;
    tbl->num_bins = size-1;
    tbl->bins = (tsf_st_table_entry **)Calloc(size, sizeof(tsf_st_table_entry*));
    if (tbl->bins==NULL) {
        free(tbl);
        return NULL;
    }

    return tbl;
}

tsf_st_table*
tsf_st_init_table(type)
    struct tsf_st_hash_type *type;
{
    return tsf_st_init_table_with_size(type, 0);
}

tsf_st_table*
tsf_st_init_numtable()
{
    return tsf_st_init_table(&type_numhash);
}

tsf_st_table*
tsf_st_init_numtable_with_size(size)
    int size;
{
    return tsf_st_init_table_with_size(&type_numhash, size);
}

tsf_st_table*
tsf_st_init_ptrtable()
{
    return tsf_st_init_table(&type_ptrhash);
}

tsf_st_table*
tsf_st_init_ptrtable_with_size(size)
    int size;
{
    return tsf_st_init_table_with_size(&type_ptrhash, size);
}

tsf_st_table*
tsf_st_init_strtable()
{
    return tsf_st_init_table(&type_strhash);
}

tsf_st_table*
tsf_st_init_strtable_with_size(size)
    int size;
{
    return tsf_st_init_table_with_size(&type_strhash, size);
}

void
tsf_st_free_table(table)
    tsf_st_table *table;
{
    register tsf_st_table_entry *ptr, *next;
    int i;

    for(i = 0; i <= table->num_bins; i++) {
        ptr = table->bins[i];
        while (ptr != 0) {
            next = ptr->next;
            free(ptr);
            ptr = next;
        }
    }
    free(table->bins);
    free(table);
}

#define PTR_NOT_EQUAL(table, ptr, hash_val, key) \
((ptr) != 0 && (ptr->hash != (hash_val) || !EQUAL((table), (key), (ptr)->key)))

#ifdef HASH_LOG
#define COLLISION collision++
#else
#define COLLISION
#endif

#define FIND_ENTRY(table, ptr, hash_val, bin_pos) \
bin_pos = hash_val&(table)->num_bins;\
ptr = (table)->bins[bin_pos];\
if (PTR_NOT_EQUAL(table, ptr, hash_val, key)) {\
    COLLISION;\
    while (PTR_NOT_EQUAL(table, ptr->next, hash_val, key)) {\
        ptr = ptr->next;\
    }\
    ptr = ptr->next;\
}

int
tsf_st_lookup(table, key, value)
    tsf_st_table *table;
    register char *key;
    void **value;
{
    unsigned int hash_val, bin_pos;
    register tsf_st_table_entry *ptr;

    hash_val = do_hash(key, table);
    FIND_ENTRY(table, ptr, hash_val, bin_pos);

    if (ptr == 0) {
        return 0;
    } else {
        if (value != 0)  *value = ptr->record;
        return 1;
    }
}

#define ADD_DIRECT(table, key, value, hash_val, bin_pos)\
{\
    tsf_st_table_entry *entry;\
    if (table->num_entries/(table->num_bins+1) > TSF_ST_DEFAULT_MAX_DENSITY) {\
        if (rehash(table)<0) {\
            return -1;\
        }\
        bin_pos = hash_val & table->num_bins;\
    }\
    \
    entry = alloc(tsf_st_table_entry);\
    if (entry==NULL) {\
        return -1;\
    }\
    \
    entry->hash = hash_val;\
    entry->key = key;\
    entry->record = value;\
    entry->next = table->bins[bin_pos];\
    table->bins[bin_pos] = entry;\
    table->num_entries++;\
}

/* make this function return -1 on error! */
int
tsf_st_insert(table, key, value)
    register tsf_st_table *table;
    register char *key;
    void *value;
{
    unsigned int hash_val, bin_pos;
    register tsf_st_table_entry *ptr;

    hash_val = do_hash(key, table);
    FIND_ENTRY(table, ptr, hash_val, bin_pos);

    if (ptr == 0) {
        ADD_DIRECT(table, key, value, hash_val, bin_pos);
        return 0;
    } else {
        ptr->record = value;
        return 1;
    }
}

/* make this function return -1 on error! */
int
tsf_st_insert_noreplace(table, key, value)
    register tsf_st_table *table;
    register char *key;
    void *value;
{
    unsigned int hash_val, bin_pos;
    register tsf_st_table_entry *ptr;

    hash_val = do_hash(key, table);
    FIND_ENTRY(table, ptr, hash_val, bin_pos);

    if (ptr == 0) {
        ADD_DIRECT(table, key, value, hash_val, bin_pos);
        return 0;
    } else {
        return 1;
    }
}

/* make this function return -1 on error! */
int
tsf_st_add_direct(table, key, value)
    tsf_st_table *table;
    char *key;
    void *value;
{
    unsigned int hash_val, bin_pos;

    hash_val = do_hash(key, table);
    bin_pos = hash_val & table->num_bins;
    ADD_DIRECT(table, key, value, hash_val, bin_pos);
    
    return 0;
}

static int
rehash(table)
    register tsf_st_table *table;
{
    register tsf_st_table_entry *ptr, *next, **new_bins;
    int i, old_num_bins = table->num_bins, new_num_bins;
    unsigned int hash_val;

    new_num_bins = new_size(old_num_bins+1);
    new_bins = (tsf_st_table_entry**)Calloc(new_num_bins, sizeof(tsf_st_table_entry*));
    if (new_bins==NULL) {
        return -1;
    }

    new_num_bins--;
    for(i = 0; i <= old_num_bins; i++) {
        ptr = table->bins[i];
        while (ptr != 0) {
            next = ptr->next;
            hash_val = ptr->hash & new_num_bins;
            ptr->next = new_bins[hash_val];
            new_bins[hash_val] = ptr;
            ptr = next;
        }
    }
    free(table->bins);
    table->num_bins = new_num_bins;
    table->bins = new_bins;
    
    return 0;
}

tsf_st_table*
tsf_st_copy(old_table)
    tsf_st_table *old_table;
{
    tsf_st_table *new_table;
    tsf_st_table_entry *ptr, *entry;
    int i, num_bins = old_table->num_bins+1;

    new_table = alloc(tsf_st_table);
    if (new_table == NULL) {
        return NULL;
    }

    *new_table = *old_table;
    new_table->bins = (tsf_st_table_entry**)
        Calloc((unsigned)num_bins, sizeof(tsf_st_table_entry*));

    if (new_table->bins == NULL) {
        free(new_table);
        return NULL;
    }

    for(i = 0; i < num_bins; i++) {
        new_table->bins[i] = 0;
        ptr = old_table->bins[i];
        while (ptr != 0) {
            entry = alloc(tsf_st_table_entry);
            if (entry == NULL) {
                free(new_table->bins);
                free(new_table);
                return NULL;
            }
            *entry = *ptr;
            entry->next = new_table->bins[i];
            new_table->bins[i] = entry;
            ptr = ptr->next;
        }
    }
    return new_table;
}

int
tsf_st_delete(table, key, value)
    register tsf_st_table *table;
    register char **key;
    void **value;
{
    unsigned int hash_val;
    tsf_st_table_entry *tmp;
    register tsf_st_table_entry *ptr;

    hash_val = do_hash_bin(*key, table);
    ptr = table->bins[hash_val];

    if (ptr == 0) {
        if (value != 0) *value = 0;
        return 0;
    }

    if (EQUAL(table, *key, ptr->key)) {
        table->bins[hash_val] = ptr->next;
        table->num_entries--;
        if (value != 0) *value = ptr->record;
        *key = ptr->key;
        free(ptr);
        return 1;
    }

    for(; ptr->next != 0; ptr = ptr->next) {
        if (EQUAL(table, ptr->next->key, *key)) {
            tmp = ptr->next;
            ptr->next = ptr->next->next;
            table->num_entries--;
            if (value != 0) *value = tmp->record;
            *key = tmp->key;
            free(tmp);
            return 1;
        }
    }

    return 0;
}

int
tsf_st_delete_safe(table, key, value, never)
    register tsf_st_table *table;
    register char **key;
    void **value;
    char *never;
{
    unsigned int hash_val;
    register tsf_st_table_entry *ptr;

    hash_val = do_hash_bin(*key, table);
    ptr = table->bins[hash_val];

    if (ptr == 0) {
        if (value != 0) *value = 0;
        return 0;
    }

    for(; ptr != 0; ptr = ptr->next) {
        if ((ptr->key != never) && EQUAL(table, ptr->key, *key)) {
            table->num_entries--;
            *key = ptr->key;
            if (value != 0) *value = ptr->record;
            ptr->key = ptr->record = never;
            return 1;
        }
    }

    return 0;
}

static int
delete_never(key, value, never)
    char *key, *never;
    void *value;
{
    if (value == never) return TSF_ST_DELETE;
    return TSF_ST_CONTINUE;
}

void
tsf_st_cleanup_safe(table, never)
    tsf_st_table *table;
    char *never;
{
    int num_entries = table->num_entries;

    tsf_st_foreach(table, (tsf_st_func_t)delete_never, never);
    table->num_entries = num_entries;
}

void
tsf_st_foreach(table, func, arg)
    tsf_st_table *table;
    tsf_st_func_t func;
    void *arg;
{
    tsf_st_table_entry *ptr, *last, *tmp;
    enum tsf_st_retval retval;
    int i;

    for(i = 0; i <= table->num_bins; i++) {
        last = 0;
        for(ptr = table->bins[i]; ptr != 0;) {
            retval = (*func)(ptr->key, ptr->record, arg);
            switch (retval) {
            case TSF_ST_CONTINUE:
                last = ptr;
                ptr = ptr->next;
                break;
            case TSF_ST_STOP:
                return;
            case TSF_ST_DELETE:
                tmp = ptr;
                if (last == 0) {
                    table->bins[i] = ptr->next;
                } else {
                    last->next = ptr->next;
                }
                ptr = ptr->next;
                free(tmp);
                table->num_entries--;
            }
        }
    }
}

void tsf_st_iterator_init(tsf_st_table *table,
                          tsf_st_iterator *iter) {
    iter->table=table;
    iter->i=0;
    iter->ptr=NULL;
    tsf_st_iterator_next(iter);
}

void tsf_st_iterator_get(tsf_st_iterator *iter,
                         char **key,
                         void **value) {
    if (key!=NULL) {
        *key=iter->ptr->key;
    }
    if (value!=NULL) {
        *value=iter->ptr->record;
    }
}

void tsf_st_iterator_next(tsf_st_iterator *iter) {
    if (iter->ptr!=NULL) {
        iter->ptr=iter->ptr->next;
    }
    if (iter->ptr==NULL) {
        for (;iter->ptr==NULL && iter->i<=iter->table->num_bins;
             ++iter->i) {
            iter->ptr=iter->table->bins[iter->i];
        }
    }
}

int tsf_st_iterator_valid(tsf_st_iterator *iter) {
    return iter->ptr!=NULL;
}

int tsf_st_strhash(string)
    register char *string;
{
    register int c;

#ifdef HASH_ELFHASH
    register unsigned int h = 0, g;

    while ((c = *string++) != '\0') {
        h = ( h << 4 ) + c;
        if ( g = h & 0xF0000000 )
            h ^= g >> 24;
        h &= ~g;
    }
    return h;
#elif HASH_PERL
    register int val = 0;

    while ((c = *string++) != '\0') {
        val = val*33 + c;
    }

    return val + (val>>5);
#else
    register int val = 0;

    while ((c = *string++) != '\0') {
        val = val*997 + c;
    }

    return val + (val>>5);
#endif
}

static int
numcmp(x, y)
    long x, y;
{
    return x != y;
}

static int
numhash(n)
    long n;
{
    return n;
}

static int ptrcmp(void *x, void *y) {
    return x != y;
}

