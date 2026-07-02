#ifndef _DEPOOLHASHARRAY_H
#define _DEPOOLHASHARRAY_H
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
 * \brief Memory pool hash-array class.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "dePoolHash.h"
#include "dePoolArray.h"

DE_BEGIN_EXTERN_C

void dePoolHashArray_selfTest(void);

DE_END_EXTERN_C

/*--------------------------------------------------------------------*//*!
 * \brief Declare a template pool hash-array (array with hash) class interface.
 * \param TYPENAME            Type name of the declared hash-array.
 * \param KEYTYPE            Type of the key.
 * \param VALUETYPE            Type of the value.
 * \param KEYARRAYTYPE        Type of the key array.
 * \param VALUEARRAYTYPE    Type of the value array.
 *
 * \todo [petri] Description.
 *
 * The functions for operating the hash are:
 * \todo [petri] Figure out how to comment these in Doxygen-style.
 *
 * \todo [pyry] HashArray_find() will break if dePoolArray implementation changes.
 *
 * \code
 * HashArray*  HashArray_create            (deMemPool* pool);
 * int         HashArray_getNumElements    (const HashArray* hashArray);
 * Value*      HashArray_find              (Hash* hashArray, Key key);
 * bool      HashArray_insert            (Hash* hashArray, Key key, Value value);
 * bool      HashArray_copyToArray       (Hash* hashArray, KeyArray* keys, ValueArray* values);
 * \endcode
*//*--------------------------------------------------------------------*/
#define DE_DECLARE_POOL_HASH_ARRAY(TYPENAME, KEYTYPE, VALUETYPE, KEYARRAYTYPE, VALUEARRAYTYPE)           \
                                                                                                         \
    DE_DECLARE_POOL_ARRAY(TYPENAME##Array, VALUETYPE);                                                   \
    DE_DECLARE_POOL_HASH(TYPENAME##Hash, KEYTYPE, int);                                                  \
                                                                                                         \
    typedef struct TYPENAME_s                                                                            \
    {                                                                                                    \
        TYPENAME##Hash *hash;                                                                            \
        TYPENAME##Array *array;                                                                          \
    } TYPENAME; /* NOLINT(TYPENAME) */                                                                   \
                                                                                                         \
    TYPENAME *TYPENAME##_create(deMemPool *pool);                                                        \
    bool TYPENAME##_insert(DE_PTR_TYPE(TYPENAME) hashArray, KEYTYPE key, VALUETYPE value);               \
    bool TYPENAME##_copyToArray(const TYPENAME *hashArray, DE_PTR_TYPE(KEYARRAYTYPE) keys,               \
                                DE_PTR_TYPE(VALUEARRAYTYPE) values);                                     \
                                                                                                         \
    static inline int TYPENAME##_getNumElements(const TYPENAME *hashArray) DE_UNUSED_FUNCTION;           \
    static inline VALUETYPE *TYPENAME##_find(const TYPENAME *hashArray, KEYTYPE key) DE_UNUSED_FUNCTION; \
    static inline void TYPENAME##_reset(DE_PTR_TYPE(TYPENAME) hashArray) DE_UNUSED_FUNCTION;             \
                                                                                                         \
    static inline int TYPENAME##_getNumElements(const TYPENAME *hashArray)                               \
    {                                                                                                    \
        return TYPENAME##Array_getNumElements(hashArray->array);                                         \
    }                                                                                                    \
                                                                                                         \
    static inline VALUETYPE *TYPENAME##_find(const TYPENAME *hashArray, KEYTYPE key)                     \
    {                                                                                                    \
        int *ndxPtr = TYPENAME##Hash_find(hashArray->hash, key);                                         \
        if (!ndxPtr)                                                                                     \
            return NULL;                                                                                 \
        else                                                                                             \
        {                                                                                                \
            int ndx = *ndxPtr;                                                                           \
            DE_ASSERT(ndx >= 0 && ndx < hashArray->array->numElements);                                  \
            {                                                                                            \
                int pageNdx = (ndx >> DE_ARRAY_ELEMENTS_PER_PAGE_LOG2);                                  \
                int subNdx  = ndx & ((1 << DE_ARRAY_ELEMENTS_PER_PAGE_LOG2) - 1);                        \
                return &((VALUETYPE *)hashArray->array->pageTable[pageNdx])[subNdx];                     \
            }                                                                                            \
        }                                                                                                \
    }                                                                                                    \
                                                                                                         \
    static inline void TYPENAME##_reset(DE_PTR_TYPE(TYPENAME) hashArray)                                 \
    {                                                                                                    \
        TYPENAME##Hash_reset(hashArray->hash);                                                           \
        TYPENAME##Array_reset(hashArray->array);                                                         \
    }                                                                                                    \
                                                                                                         \
    struct TYPENAME##Unused_s                                                                            \
    {                                                                                                    \
        int unused;                                                                                      \
    }

/*--------------------------------------------------------------------*//*!
 * \brief Implement a template pool hash-array class.
 * \param TYPENAME            Type name of the declared hash.
 * \param KEYTYPE            Type of the key.
 * \param VALUETYPE            Type of the value.
 * \param KEYARRAYTYPE        Type of the key array.
 * \param VALUEARRAYTYPE    Type of the value array.
 * \param HASHFUNC            Function used for hashing the key.
 * \param CMPFUNC            Function used for exact matching of the keys.
 *
 * This macro has implements the hash declared with DE_DECLARE_POOL_HASH.
 * Usually this macro should be used from a .c file, since the macro expands
 * into multiple functions. The TYPENAME, KEYTYPE, and VALUETYPE parameters
 * must match those of the declare macro.
*//*--------------------------------------------------------------------*/
#define DE_IMPLEMENT_POOL_HASH_ARRAY(TYPENAME, KEYTYPE, VALUETYPE, KEYARRAYTYPE, VALUEARRAYTYPE, KEYHASHFUNC,         \
                                     KEYCMPFUNC)                                                                      \
                                                                                                                      \
    DE_IMPLEMENT_POOL_HASH(TYPENAME##Hash, KEYTYPE, int, KEYHASHFUNC, KEYCMPFUNC);                                    \
                                                                                                                      \
    TYPENAME *TYPENAME##_create(deMemPool *pool)                                                                      \
    {                                                                                                                 \
        DE_PTR_TYPE(TYPENAME) hashArray = DE_POOL_NEW(pool, TYPENAME);                                                \
        if (!hashArray)                                                                                               \
            return NULL;                                                                                              \
        if ((hashArray->hash = TYPENAME##Hash_create(pool)) == NULL)                                                  \
            return NULL;                                                                                              \
        if ((hashArray->array = TYPENAME##Array_create(pool)) == NULL)                                                \
            return NULL;                                                                                              \
        return hashArray;                                                                                             \
    }                                                                                                                 \
                                                                                                                      \
    bool TYPENAME##_insert(DE_PTR_TYPE(TYPENAME) hashArray, KEYTYPE key, VALUETYPE value)                             \
    {                                                                                                                 \
        int numElements = TYPENAME##Array_getNumElements(hashArray->array);                                           \
        DE_ASSERT(TYPENAME##Hash_getNumElements(hashArray->hash) == numElements);                                     \
        DE_ASSERT(!TYPENAME##Hash_find(hashArray->hash, key));                                                        \
        if (!TYPENAME##Array_setSize(hashArray->array, numElements + 1) ||                                            \
            !TYPENAME##Hash_insert(hashArray->hash, key, numElements))                                                \
            return false;                                                                                             \
        TYPENAME##Array_set(hashArray->array, numElements, value);                                                    \
        return true;                                                                                                  \
    }                                                                                                                 \
                                                                                                                      \
    bool TYPENAME##_copyToArray(const TYPENAME *hashArray, DE_PTR_TYPE(KEYARRAYTYPE) keys,                            \
                                DE_PTR_TYPE(VALUEARRAYTYPE) values)                                                   \
    {                                                                                                                 \
        int numElements      = TYPENAME##Array_getNumElements(hashArray->array);                                      \
        TYPENAME##Hash *hash = hashArray->hash;                                                                       \
        TYPENAME##HashIter iter;                                                                                      \
        DE_ASSERT(TYPENAME##Hash_getNumElements(hashArray->hash) == numElements);                                     \
        if ((keys && !KEYARRAYTYPE##_setSize(keys, numElements)) ||                                                   \
            (values && !VALUEARRAYTYPE##_setSize(values, numElements)))                                               \
            return false;                                                                                             \
        for (TYPENAME##HashIter_init(hash, &iter); TYPENAME##HashIter_hasItem(&iter); TYPENAME##HashIter_next(&iter)) \
        {                                                                                                             \
            KEYTYPE key = TYPENAME##HashIter_getKey(&iter);                                                           \
            int ndx     = TYPENAME##HashIter_getValue(&iter);                                                         \
            if (keys)                                                                                                 \
                KEYARRAYTYPE##_set(keys, ndx, key);                                                                   \
            if (values)                                                                                               \
                VALUEARRAYTYPE##_set(values, ndx, TYPENAME##Array_get(hashArray->array, ndx));                        \
        }                                                                                                             \
        return true;                                                                                                  \
    }                                                                                                                 \
                                                                                                                      \
    struct TYPENAME##Unused2_s                                                                                        \
    {                                                                                                                 \
        int unused;                                                                                                   \
    }

#endif /* _DEPOOLHASHARRAY_H */
