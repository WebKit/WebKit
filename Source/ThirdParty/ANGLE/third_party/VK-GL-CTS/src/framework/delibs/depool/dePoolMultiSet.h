#ifndef _DEPOOLMULTISET_H
#define _DEPOOLMULTISET_H
/*-------------------------------------------------------------------------
 * drawElements Memory Pool Library
 * --------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Memory pool multiset class.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "deMemPool.h"
#include "dePoolHash.h"
#include "deInt32.h"

DE_BEGIN_EXTERN_C

void dePoolMultiSet_selfTest(void);

DE_END_EXTERN_C

/*--------------------------------------------------------------------*//*!
 * \brief Declare a template pool multiset class interface.
 * \param TYPENAME    Type name of the declared multiset.
 * \param KEYTYPE    Type of the key.
 *
 * This macro declares the interface for a multiset. For the implementation
 * of the multiset, see DE_IMPLEMENT_POOL_MULTISET. Usually this macro is put
 * into the header file and the implementation macro is put in some .c file.
 *
 * \todo [petri] Detailed description.
 *
 * The functions for operating the multiset are:
 * \todo [petri] Figure out how to comment these in Doxygen-style.
 *
 * \code
 * MultiSet* MultiSet_create            (deMemPool* pool);
 * int       MultiSet_getNumElements    (const MultiSet* set);
 * bool    MultiSet_exists            (const MultiSet* set, Key key);
 * bool    MultiSet_insert            (MultiSet* set, Key key);
 * void      MultiSet_delete            (MultiSet* set, Key key);
 * int       MultiSet_getKeyCount       (const MultiSet* set, Key key);
 * bool    MultiSet_setKeyCount       (MultiSet* set, Key key, int count);
 * \endcode
*//*--------------------------------------------------------------------*/
#define DE_DECLARE_POOL_MULTISET(TYPENAME, KEYTYPE)                                    \
                                                                                       \
    DE_DECLARE_POOL_HASH(TYPENAME##Hash, KEYTYPE, int);                                \
                                                                                       \
    typedef struct TYPENAME##_s                                                        \
    {                                                                                  \
        deMemPool *pool;                                                               \
        int numElements;                                                               \
        TYPENAME##Hash *hash;                                                          \
    } TYPENAME; /* NOLINT(TYPENAME) */                                                 \
                                                                                       \
    TYPENAME *TYPENAME##_create(deMemPool *pool);                                      \
    void TYPENAME##_reset(DE_PTR_TYPE(TYPENAME) set);                                  \
    bool TYPENAME##_setKeyCount(DE_PTR_TYPE(TYPENAME) set, KEYTYPE key, int newCount); \
                                                                                       \
    static inline int TYPENAME##_getNumElements(const TYPENAME *set)                   \
    {                                                                                  \
        return set->numElements;                                                       \
    }                                                                                  \
                                                                                       \
    static inline int TYPENAME##_getKeyCount(const TYPENAME *set, KEYTYPE key)         \
    {                                                                                  \
        int *countPtr = TYPENAME##Hash_find(set->hash, key);                           \
        int count     = countPtr ? *countPtr : 0;                                      \
        DE_ASSERT(count > 0 || !countPtr);                                             \
        return count;                                                                  \
    }                                                                                  \
                                                                                       \
    static inline bool TYPENAME##_exists(const TYPENAME *set, KEYTYPE key)             \
    {                                                                                  \
        return (TYPENAME##_getKeyCount(set, key) > 0);                                 \
    }                                                                                  \
                                                                                       \
    static inline bool TYPENAME##_insert(DE_PTR_TYPE(TYPENAME) set, KEYTYPE key)       \
    {                                                                                  \
        int oldCount = TYPENAME##_getKeyCount(set, key);                               \
        return TYPENAME##_setKeyCount(set, key, oldCount + 1);                         \
    }                                                                                  \
                                                                                       \
    static inline void TYPENAME##_delete(DE_PTR_TYPE(TYPENAME) set, KEYTYPE key)       \
    {                                                                                  \
        int oldCount = TYPENAME##_getKeyCount(set, key);                               \
        DE_ASSERT(oldCount > 0);                                                       \
        TYPENAME##_setKeyCount(set, key, oldCount - 1);                                \
    }                                                                                  \
                                                                                       \
    struct TYPENAME##DeclareUnused_s                                                   \
    {                                                                                  \
        int unused;                                                                    \
    }

/*--------------------------------------------------------------------*//*!
 * \brief Implement a template pool multiset class.
 * \param TYPENAME    Type name of the declared multiset.
 * \param KEYTYPE    Type of the key.
 * \param HASHFUNC    Function used for hashing the key.
 * \param CMPFUNC    Function used for exact matching of the keys.
 *
 * This macro has implements the set declared with DE_DECLARE_POOL_MULTISET.
 * Usually this macro should be used from a .c file, since the macro expands
 * into multiple functions. The TYPENAME and KEYTYPE parameters
 * must match those of the declare macro.
*//*--------------------------------------------------------------------*/
#define DE_IMPLEMENT_POOL_MULTISET(TYPENAME, KEYTYPE, HASHFUNC, CMPFUNC)              \
                                                                                      \
    DE_IMPLEMENT_POOL_HASH(TYPENAME##Hash, KEYTYPE, int, HASHFUNC, CMPFUNC);          \
                                                                                      \
    TYPENAME *TYPENAME##_create(deMemPool *pool)                                      \
    {                                                                                 \
        /* Alloc struct. */                                                           \
        DE_PTR_TYPE(TYPENAME) set = DE_POOL_NEW(pool, TYPENAME);                      \
        if (!set)                                                                     \
            return NULL;                                                              \
                                                                                      \
        /* Init. */                                                                   \
        memset(set, 0, sizeof(TYPENAME));                                             \
        set->pool = pool;                                                             \
                                                                                      \
        set->hash = TYPENAME##Hash_create(pool);                                      \
                                                                                      \
        return set;                                                                   \
    }                                                                                 \
                                                                                      \
    void TYPENAME##_reset(DE_PTR_TYPE(TYPENAME) set)                                  \
    {                                                                                 \
        TYPENAME##Hash_reset(set->hash);                                              \
        set->numElements = 0;                                                         \
    }                                                                                 \
                                                                                      \
    bool TYPENAME##_setKeyCount(DE_PTR_TYPE(TYPENAME) set, KEYTYPE key, int newCount) \
    {                                                                                 \
        int *countPtr = TYPENAME##Hash_find(set->hash, key);                          \
        int oldCount  = countPtr ? *countPtr : 0;                                     \
                                                                                      \
        DE_ASSERT(oldCount > 0 || !countPtr);                                         \
        DE_ASSERT(newCount >= 0);                                                     \
        set->numElements += (newCount - oldCount);                                    \
                                                                                      \
        if (newCount == 0 && countPtr)                                                \
            TYPENAME##Hash_delete(set->hash, key);                                    \
        else if (newCount > 0 && countPtr)                                            \
            *countPtr = newCount;                                                     \
        else if (newCount > 0)                                                        \
            return TYPENAME##Hash_insert(set->hash, key, newCount);                   \
        return true;                                                                  \
    }                                                                                 \
                                                                                      \
    struct TYPENAME##ImplementUnused_s                                                \
    {                                                                                 \
        int unused;                                                                   \
    }

/*--------------------------------------------------------------------*//*!
 * \brief Declare set-wise operations for a multiset template.
 * \param TYPENAME    Type name of the declared set.
 * \param KEYTYPE    Type of the key.
 *
 * This macro declares union and intersection operations for a multiset.
 * For implementation see DE_IMPLEMENT_POOL_MULTISET_UNION_INTERSECT.
 *
 * \todo [petri] Detailed description.
 *
 * The functions for operating the set are:
 * \todo [petri] Figure out how to comment these in Doxygen-style.
 *
 * \code
 * bool    MultiSet_union                (Set* to, const Set* a, const Set* b);
 * bool    MultiSet_unionInplace        (Set* a, const Set* b);
 * bool    MultiSet_intersect            (Set* to, const Set* a, const Set* b);
 * void        MultiSet_intersectInplace    (Set* a, const Set* b);
 * bool   MultiSet_sum                (Set* to, const Set* a, const Set* b);
 * bool   MultiSet_sumInplace            (Set* a, const Set* b);
 * bool   MultiSet_difference            (Set* to, const Set* a, const Set* b);
 * void        MultiSet_differenceInplace    (Set* a, const Set* b);
 * \endcode
*//*--------------------------------------------------------------------*/
#define DE_DECLARE_POOL_MULTISET_SETWISE_OPERATIONS(TYPENAME)                                   \
    bool TYPENAME##_union(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b);      \
    bool TYPENAME##_unionInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b);                   \
    bool TYPENAME##_intersect(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b);  \
    void TYPENAME##_intersectInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b);               \
    bool TYPENAME##_sum(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b);        \
    bool TYPENAME##_sumInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b);                     \
    bool TYPENAME##_difference(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b); \
    void TYPENAME##_differenceInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b);              \
    struct TYPENAME##SetwiseDeclareUnused_s                                                     \
    {                                                                                           \
        int unused;                                                                             \
    }

#define DE_IMPLEMENT_POOL_MULTISET_SETWISE_OPERATIONS(TYPENAME, KEYTYPE)                                           \
    bool TYPENAME##_union(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b)                          \
    {                                                                                                              \
        TYPENAME##_reset(to);                                                                                      \
        return TYPENAME##_unionInplace(to, a) && TYPENAME##_unionInplace(to, b);                                   \
    }                                                                                                              \
                                                                                                                   \
    bool TYPENAME##_unionInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b)                                       \
    {                                                                                                              \
        TYPENAME##HashIter iter;                                                                                   \
        for (TYPENAME##HashIter_init(b, &iter); TYPENAME##HashIter_hasItem(&iter); TYPENAME##HashIter_next(&iter)) \
        {                                                                                                          \
            KEYTYPE key = TYPENAME##HashIter_getKey(&iter);                                                        \
            int bCount  = TYPENAME##HashIter_getValue(&iter);                                                      \
            int aCount  = TYPENAME##_getKeyCount(a, key);                                                          \
            int count   = deMax32(aCount, bCount);                                                                 \
            if (bCount && !TYPENAME##_setKeyCount(a, key, aCount + bCount))                                        \
                return false;                                                                                      \
        }                                                                                                          \
        return true;                                                                                               \
    }                                                                                                              \
                                                                                                                   \
    bool TYPENAME##_intersect(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b)                      \
    {                                                                                                              \
        TYPENAME##HashIter iter;                                                                                   \
        TYPENAME##_reset(to);                                                                                      \
        for (TYPENAME##HashIter_init(a, &iter); TYPENAME##HashIter_hasItem(&iter); TYPENAME##HashIter_next(&iter)) \
        {                                                                                                          \
            KEYTYPE key = TYPENAME##HashIter_getKey(&iter);                                                        \
            int aCount  = TYPENAME##HashIter_getValue(&iter);                                                      \
            int bCount  = TYPENAME##_getKeyValue(b, key);                                                          \
            int count   = deMin32(aCount, bCount);                                                                 \
            if (count && !TYPENAME##_setKeyCount(to, key, count))                                                  \
                return false;                                                                                      \
        }                                                                                                          \
        return true;                                                                                               \
    }                                                                                                              \
                                                                                                                   \
    void TYPENAME##_intersectInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b)                                   \
    {                                                                                                              \
        DE_FATAL("Not implemented.");                                                                              \
    }                                                                                                              \
                                                                                                                   \
    bool TYPENAME##_sum(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b)                            \
    {                                                                                                              \
        TYPENAME##_reset(to);                                                                                      \
        return TYPENAME##_sumInplace(to, a) && TYPENAME##_sumInplace(to, b);                                       \
    }                                                                                                              \
                                                                                                                   \
    bool TYPENAME##_sumInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b)                                         \
    {                                                                                                              \
        TYPENAME##HashIter iter;                                                                                   \
        for (TYPENAME##HashIter_init(b, &iter); TYPENAME##HashIter_hasItem(&iter); TYPENAME##HashIter_next(&iter)) \
        {                                                                                                          \
            KEYTYPE key = TYPENAME##HashIter_getKey(&iter);                                                        \
            int aCount  = TYPENAME##_getKeyValue(a, key);                                                          \
            int bCount  = TYPENAME##HashIter_getValue(&iter);                                                      \
            int count   = aCount + bCount;                                                                         \
            if (!TYPENAME##_setKeyCount(a, key, count))                                                            \
                return false;                                                                                      \
        }                                                                                                          \
    }                                                                                                              \
                                                                                                                   \
    bool TYPENAME##_difference(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b)                     \
    {                                                                                                              \
        TYPENAME##HashIter iter;                                                                                   \
        TYPENAME##_reset(to);                                                                                      \
        for (TYPENAME##HashIter_init(a, &iter); TYPENAME##HashIter_hasItem(&iter); TYPENAME##HashIter_next(&iter)) \
        {                                                                                                          \
            KEYTYPE key = TYPENAME##HashIter_getKey(&iter);                                                        \
            int aCount  = TYPENAME##HashIter_getValue(&iter);                                                      \
            int bCount  = TYPENAME##_getKeyValue(b, key);                                                          \
            int count   = deMax32(0, aCount - bCount);                                                             \
            if (count && !TYPENAME##_setKeyCount(to, key, count))                                                  \
                return false;                                                                                      \
        }                                                                                                          \
        return true;                                                                                               \
    }                                                                                                              \
                                                                                                                   \
    void TYPENAME##_differenceInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b)                                  \
    {                                                                                                              \
        DE_FATAL("Not implemented.");                                                                              \
    }                                                                                                              \
                                                                                                                   \
    struct TYPENAME##SetwiseImplementUnused_s                                                                      \
    {                                                                                                              \
        int unused;                                                                                                \
    }

#endif /* _DEPOOLMULTISET_H */
