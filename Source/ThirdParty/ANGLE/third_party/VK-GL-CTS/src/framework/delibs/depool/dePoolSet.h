#ifndef _DEPOOLSET_H
#define _DEPOOLSET_H
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
 * \brief Memory pool set class.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"
#include "deMemPool.h"
#include "dePoolArray.h"
#include "deInt32.h"

#include <string.h> /* memset() */

enum
{
    DE_SET_ELEMENTS_PER_SLOT = 4
};

DE_BEGIN_EXTERN_C

void dePoolSet_selfTest(void);

DE_END_EXTERN_C

/*--------------------------------------------------------------------*//*!
 * \brief Declare a template pool set class interface.
 * \param TYPENAME    Type name of the declared set.
 * \param KEYTYPE    Type of the key.
 *
 * This macro declares the interface for a set. For the implementation of
 * the set, see DE_IMPLEMENT_POOL_HASH. Usually this macro is put into the
 * header file and the implementation macro is put in some .c file.
 *
 * \todo [petri] Detailed description.
 *
 * The functions for operating the set are:
 * \todo [petri] Figure out how to comment these in Doxygen-style.
 *
 * \code
 * Set*     Set_create            (deMemPool* pool);
 * int      Set_getNumElements    (const Set* array);
 * bool   Set_exists            (const Set* array, Key key);
 * bool   Set_insert            (Set* array, Key key);
 * void     Set_delete            (Set* array, Key key);
 * \endcode
*//*--------------------------------------------------------------------*/
#define DE_DECLARE_POOL_SET(TYPENAME, KEYTYPE)                                                             \
                                                                                                           \
    typedef struct TYPENAME##Slot_s TYPENAME##Slot;                                                        \
                                                                                                           \
    struct TYPENAME##Slot_s                                                                                \
    {                                                                                                      \
        int numUsed;                                                                                       \
        TYPENAME##Slot *nextSlot;                                                                          \
        KEYTYPE keys[DE_SET_ELEMENTS_PER_SLOT];                                                            \
    };                                                                                                     \
                                                                                                           \
    typedef struct TYPENAME##_s                                                                            \
    {                                                                                                      \
        deMemPool *pool;                                                                                   \
        int numElements;                                                                                   \
                                                                                                           \
        int slotTableSize;                                                                                 \
        TYPENAME##Slot **slotTable;                                                                        \
        TYPENAME##Slot *slotFreeList;                                                                      \
    } TYPENAME; /* NOLINT(TYPENAME) */                                                                     \
                                                                                                           \
    typedef struct TYPENAME##Iter_s                                                                        \
    {                                                                                                      \
        const TYPENAME *hash;                                                                              \
        int curSlotIndex;                                                                                  \
        const TYPENAME##Slot *curSlot;                                                                     \
        int curElemIndex;                                                                                  \
    } TYPENAME##Iter;                                                                                      \
                                                                                                           \
    TYPENAME *TYPENAME##_create(deMemPool *pool);                                                          \
    void TYPENAME##_reset(DE_PTR_TYPE(TYPENAME) set);                                                      \
    bool TYPENAME##_reserve(DE_PTR_TYPE(TYPENAME) set, int capacity);                                      \
    bool TYPENAME##_exists(const TYPENAME *set, KEYTYPE key);                                              \
    bool TYPENAME##_insert(DE_PTR_TYPE(TYPENAME) set, KEYTYPE key);                                        \
    void TYPENAME##_delete(DE_PTR_TYPE(TYPENAME) set, KEYTYPE key);                                        \
                                                                                                           \
    static inline int TYPENAME##_getNumElements(const TYPENAME *set) DE_UNUSED_FUNCTION;                   \
    static inline void TYPENAME##Iter_init(const TYPENAME *hash, TYPENAME##Iter *iter) DE_UNUSED_FUNCTION; \
    static inline bool TYPENAME##Iter_hasItem(const TYPENAME##Iter *iter) DE_UNUSED_FUNCTION;              \
    static inline void TYPENAME##Iter_next(TYPENAME##Iter *iter) DE_UNUSED_FUNCTION;                       \
    static inline KEYTYPE TYPENAME##Iter_getKey(const TYPENAME##Iter *iter) DE_UNUSED_FUNCTION;            \
    static inline bool TYPENAME##_safeInsert(DE_PTR_TYPE(TYPENAME) set, KEYTYPE key) DE_UNUSED_FUNCTION;   \
    static inline void TYPENAME##_safeDelete(DE_PTR_TYPE(TYPENAME) set, KEYTYPE key) DE_UNUSED_FUNCTION;   \
                                                                                                           \
    static inline int TYPENAME##_getNumElements(const TYPENAME *set)                                       \
    {                                                                                                      \
        return set->numElements;                                                                           \
    }                                                                                                      \
                                                                                                           \
    static inline void TYPENAME##Iter_init(const TYPENAME *hash, TYPENAME##Iter *iter)                     \
    {                                                                                                      \
        iter->hash         = hash;                                                                         \
        iter->curSlotIndex = 0;                                                                            \
        iter->curSlot      = NULL;                                                                         \
        iter->curElemIndex = 0;                                                                            \
        if (TYPENAME##_getNumElements(hash) > 0)                                                           \
        {                                                                                                  \
            int slotTableSize = hash->slotTableSize;                                                       \
            int slotNdx       = 0;                                                                         \
            while (slotNdx < slotTableSize)                                                                \
            {                                                                                              \
                if (hash->slotTable[slotNdx])                                                              \
                    break;                                                                                 \
                slotNdx++;                                                                                 \
            }                                                                                              \
            DE_ASSERT(slotNdx < slotTableSize);                                                            \
            iter->curSlotIndex = slotNdx;                                                                  \
            iter->curSlot      = hash->slotTable[slotNdx];                                                 \
            DE_ASSERT(iter->curSlot);                                                                      \
        }                                                                                                  \
    }                                                                                                      \
                                                                                                           \
    static inline bool TYPENAME##Iter_hasItem(const TYPENAME##Iter *iter)                                  \
    {                                                                                                      \
        return (iter->curSlot != NULL);                                                                    \
    }                                                                                                      \
                                                                                                           \
    static inline void TYPENAME##Iter_next(TYPENAME##Iter *iter)                                           \
    {                                                                                                      \
        DE_ASSERT(TYPENAME##Iter_hasItem(iter));                                                           \
        if (++iter->curElemIndex == iter->curSlot->numUsed)                                                \
        {                                                                                                  \
            iter->curElemIndex = 0;                                                                        \
            if (iter->curSlot->nextSlot)                                                                   \
            {                                                                                              \
                iter->curSlot = iter->curSlot->nextSlot;                                                   \
            }                                                                                              \
            else                                                                                           \
            {                                                                                              \
                const TYPENAME *hash = iter->hash;                                                         \
                int curSlotIndex     = iter->curSlotIndex;                                                 \
                int slotTableSize    = hash->slotTableSize;                                                \
                while (++curSlotIndex < slotTableSize)                                                     \
                {                                                                                          \
                    if (hash->slotTable[curSlotIndex])                                                     \
                        break;                                                                             \
                }                                                                                          \
                iter->curSlotIndex = curSlotIndex;                                                         \
                if (curSlotIndex < slotTableSize)                                                          \
                    iter->curSlot = hash->slotTable[curSlotIndex];                                         \
                else                                                                                       \
                    iter->curSlot = NULL;                                                                  \
            }                                                                                              \
        }                                                                                                  \
    }                                                                                                      \
                                                                                                           \
    static inline KEYTYPE TYPENAME##Iter_getKey(const TYPENAME##Iter *iter)                                \
    {                                                                                                      \
        DE_ASSERT(TYPENAME##Iter_hasItem(iter));                                                           \
        return iter->curSlot->keys[iter->curElemIndex];                                                    \
    }                                                                                                      \
                                                                                                           \
    static inline bool TYPENAME##_safeInsert(DE_PTR_TYPE(TYPENAME) set, KEYTYPE key)                       \
    {                                                                                                      \
        DE_ASSERT(set);                                                                                    \
        if (TYPENAME##_exists(set, key))                                                                   \
            return true;                                                                                   \
        return TYPENAME##_insert(set, key);                                                                \
    }                                                                                                      \
                                                                                                           \
    static inline void TYPENAME##_safeDelete(DE_PTR_TYPE(TYPENAME) set, KEYTYPE key)                       \
    {                                                                                                      \
        DE_ASSERT(set);                                                                                    \
        if (TYPENAME##_exists(set, key))                                                                   \
            TYPENAME##_delete(set, key);                                                                   \
    }                                                                                                      \
                                                                                                           \
    struct TYPENAME##Unused_s                                                                              \
    {                                                                                                      \
        int unused;                                                                                        \
    }

/*--------------------------------------------------------------------*//*!
 * \brief Implement a template pool set class.
 * \param TYPENAME    Type name of the declared set.
 * \param KEYTYPE    Type of the key.
 * \param HASHFUNC    Function used for hashing the key.
 * \param CMPFUNC    Function used for exact matching of the keys.
 *
 * This macro has implements the set declared with DE_DECLARE_POOL_SET.
 * Usually this macro should be used from a .c file, since the macro expands
 * into multiple functions. The TYPENAME and KEYTYPE parameters
 * must match those of the declare macro.
*//*--------------------------------------------------------------------*/
#define DE_IMPLEMENT_POOL_SET(TYPENAME, KEYTYPE, HASHFUNC, CMPFUNC)                                                 \
                                                                                                                    \
    DE_PTR_TYPE(TYPENAME) TYPENAME##_create(deMemPool *pool)                                                        \
    {                                                                                                               \
        /* Alloc struct. */                                                                                         \
        DE_PTR_TYPE(TYPENAME) set = DE_POOL_NEW(pool, TYPENAME);                                                    \
        if (!set)                                                                                                   \
            return NULL;                                                                                            \
                                                                                                                    \
        /* Init array. */                                                                                           \
        memset(set, 0, sizeof(TYPENAME));                                                                           \
        set->pool = pool;                                                                                           \
                                                                                                                    \
        return set;                                                                                                 \
    }                                                                                                               \
                                                                                                                    \
    void TYPENAME##_reset(DE_PTR_TYPE(TYPENAME) set)                                                                \
    {                                                                                                               \
        int slotNdx;                                                                                                \
        for (slotNdx = 0; slotNdx < set->slotTableSize; slotNdx++)                                                  \
        {                                                                                                           \
            TYPENAME##Slot *slot = set->slotTable[slotNdx];                                                         \
            while (slot)                                                                                            \
            {                                                                                                       \
                TYPENAME##Slot *nextSlot = slot->nextSlot;                                                          \
                slot->nextSlot           = set->slotFreeList;                                                       \
                set->slotFreeList        = slot;                                                                    \
                slot->numUsed            = 0;                                                                       \
                slot                     = nextSlot;                                                                \
            }                                                                                                       \
            set->slotTable[slotNdx] = NULL;                                                                         \
        }                                                                                                           \
        set->numElements = 0;                                                                                       \
    }                                                                                                               \
                                                                                                                    \
    TYPENAME##Slot *TYPENAME##_allocSlot(DE_PTR_TYPE(TYPENAME) set)                                                 \
    {                                                                                                               \
        TYPENAME##Slot *slot;                                                                                       \
        if (set->slotFreeList)                                                                                      \
        {                                                                                                           \
            slot              = set->slotFreeList;                                                                  \
            set->slotFreeList = set->slotFreeList->nextSlot;                                                        \
        }                                                                                                           \
        else                                                                                                        \
            slot = (TYPENAME##Slot *)deMemPool_alloc(set->pool, sizeof(TYPENAME##Slot) * DE_SET_ELEMENTS_PER_SLOT); \
                                                                                                                    \
        if (slot)                                                                                                   \
        {                                                                                                           \
            slot->nextSlot = NULL;                                                                                  \
            slot->numUsed  = 0;                                                                                     \
        }                                                                                                           \
                                                                                                                    \
        return slot;                                                                                                \
    }                                                                                                               \
                                                                                                                    \
    bool TYPENAME##_rehash(DE_PTR_TYPE(TYPENAME) set, int newSlotTableSize)                                         \
    {                                                                                                               \
        DE_ASSERT(deIsPowerOfTwo32(newSlotTableSize) && newSlotTableSize > 0);                                      \
        if (newSlotTableSize > set->slotTableSize)                                                                  \
        {                                                                                                           \
            TYPENAME##Slot **oldSlotTable = set->slotTable;                                                         \
            TYPENAME##Slot **newSlotTable =                                                                         \
                (TYPENAME##Slot **)deMemPool_alloc(set->pool, sizeof(TYPENAME##Slot *) * (size_t)newSlotTableSize); \
            int oldSlotTableSize = set->slotTableSize;                                                              \
            int slotNdx;                                                                                            \
                                                                                                                    \
            if (!newSlotTable)                                                                                      \
                return false;                                                                                       \
                                                                                                                    \
            for (slotNdx = 0; slotNdx < oldSlotTableSize; slotNdx++)                                                \
                newSlotTable[slotNdx] = oldSlotTable[slotNdx];                                                      \
                                                                                                                    \
            for (slotNdx = oldSlotTableSize; slotNdx < newSlotTableSize; slotNdx++)                                 \
                newSlotTable[slotNdx] = NULL;                                                                       \
                                                                                                                    \
            set->slotTableSize = newSlotTableSize;                                                                  \
            set->slotTable     = newSlotTable;                                                                      \
                                                                                                                    \
            for (slotNdx = 0; slotNdx < oldSlotTableSize; slotNdx++)                                                \
            {                                                                                                       \
                TYPENAME##Slot *slot  = oldSlotTable[slotNdx];                                                      \
                newSlotTable[slotNdx] = NULL;                                                                       \
                while (slot)                                                                                        \
                {                                                                                                   \
                    int elemNdx;                                                                                    \
                    for (elemNdx = 0; elemNdx < slot->numUsed; elemNdx++)                                           \
                    {                                                                                               \
                        set->numElements--;                                                                         \
                        if (!TYPENAME##_insert(set, slot->keys[elemNdx]))                                           \
                            return false;                                                                           \
                    }                                                                                               \
                    slot = slot->nextSlot;                                                                          \
                }                                                                                                   \
            }                                                                                                       \
        }                                                                                                           \
                                                                                                                    \
        return true;                                                                                                \
    }                                                                                                               \
                                                                                                                    \
    bool TYPENAME##_exists(const TYPENAME *set, KEYTYPE key)                                                        \
    {                                                                                                               \
        if (set->numElements > 0)                                                                                   \
        {                                                                                                           \
            int slotNdx          = (int)(HASHFUNC(key) & (uint32_t)(set->slotTableSize - 1));                       \
            TYPENAME##Slot *slot = set->slotTable[slotNdx];                                                         \
            DE_ASSERT(deInBounds32(slotNdx, 0, set->slotTableSize));                                                \
                                                                                                                    \
            while (slot)                                                                                            \
            {                                                                                                       \
                int elemNdx;                                                                                        \
                for (elemNdx = 0; elemNdx < slot->numUsed; elemNdx++)                                               \
                {                                                                                                   \
                    if (CMPFUNC(slot->keys[elemNdx], key))                                                          \
                        return true;                                                                                \
                }                                                                                                   \
                slot = slot->nextSlot;                                                                              \
            }                                                                                                       \
        }                                                                                                           \
                                                                                                                    \
        return false;                                                                                               \
    }                                                                                                               \
                                                                                                                    \
    bool TYPENAME##_insert(DE_PTR_TYPE(TYPENAME) set, KEYTYPE key)                                                  \
    {                                                                                                               \
        int slotNdx;                                                                                                \
        TYPENAME##Slot *slot;                                                                                       \
                                                                                                                    \
        DE_ASSERT(set);                                                                                             \
        DE_ASSERT(!TYPENAME##_exists(set, key));                                                                    \
                                                                                                                    \
        if ((set->numElements + 1) >= set->slotTableSize * DE_SET_ELEMENTS_PER_SLOT)                                \
            if (!TYPENAME##_rehash(set, deMax32(4, 2 * set->slotTableSize)))                                        \
                return false;                                                                                       \
                                                                                                                    \
        slotNdx = (int)(HASHFUNC(key) & (uint32_t)(set->slotTableSize - 1));                                        \
        DE_ASSERT(slotNdx >= 0 && slotNdx < set->slotTableSize);                                                    \
        slot = set->slotTable[slotNdx];                                                                             \
                                                                                                                    \
        if (!slot)                                                                                                  \
        {                                                                                                           \
            slot = TYPENAME##_allocSlot(set);                                                                       \
            if (!slot)                                                                                              \
                return false;                                                                                       \
            set->slotTable[slotNdx] = slot;                                                                         \
        }                                                                                                           \
                                                                                                                    \
        for (;;)                                                                                                    \
        {                                                                                                           \
            if (slot->numUsed == DE_SET_ELEMENTS_PER_SLOT)                                                          \
            {                                                                                                       \
                if (slot->nextSlot)                                                                                 \
                    slot = slot->nextSlot;                                                                          \
                else                                                                                                \
                {                                                                                                   \
                    TYPENAME##Slot *nextSlot = TYPENAME##_allocSlot(set);                                           \
                    if (!nextSlot)                                                                                  \
                        return false;                                                                               \
                    slot->nextSlot = nextSlot;                                                                      \
                    slot           = nextSlot;                                                                      \
                }                                                                                                   \
            }                                                                                                       \
            else                                                                                                    \
            {                                                                                                       \
                slot->keys[slot->numUsed] = key;                                                                    \
                slot->numUsed++;                                                                                    \
                set->numElements++;                                                                                 \
                return true;                                                                                        \
            }                                                                                                       \
        }                                                                                                           \
    }                                                                                                               \
                                                                                                                    \
    void TYPENAME##_delete(DE_PTR_TYPE(TYPENAME) set, KEYTYPE key)                                                  \
    {                                                                                                               \
        int slotNdx;                                                                                                \
        TYPENAME##Slot *slot;                                                                                       \
        TYPENAME##Slot *prevSlot = NULL;                                                                            \
                                                                                                                    \
        DE_ASSERT(set->numElements > 0);                                                                            \
        slotNdx = (int)(HASHFUNC(key) & (uint32_t)(set->slotTableSize - 1));                                        \
        DE_ASSERT(slotNdx >= 0 && slotNdx < set->slotTableSize);                                                    \
        slot = set->slotTable[slotNdx];                                                                             \
        DE_ASSERT(slot);                                                                                            \
                                                                                                                    \
        for (;;)                                                                                                    \
        {                                                                                                           \
            int elemNdx;                                                                                            \
            DE_ASSERT(slot->numUsed > 0);                                                                           \
            for (elemNdx = 0; elemNdx < slot->numUsed; elemNdx++)                                                   \
            {                                                                                                       \
                if (CMPFUNC(key, slot->keys[elemNdx]))                                                              \
                {                                                                                                   \
                    TYPENAME##Slot *lastSlot = slot;                                                                \
                    while (lastSlot->nextSlot)                                                                      \
                    {                                                                                               \
                        prevSlot = lastSlot;                                                                        \
                        lastSlot = lastSlot->nextSlot;                                                              \
                    }                                                                                               \
                                                                                                                    \
                    slot->keys[elemNdx] = lastSlot->keys[lastSlot->numUsed - 1];                                    \
                    lastSlot->numUsed--;                                                                            \
                                                                                                                    \
                    if (lastSlot->numUsed == 0)                                                                     \
                    {                                                                                               \
                        if (prevSlot)                                                                               \
                            prevSlot->nextSlot = NULL;                                                              \
                        else                                                                                        \
                            set->slotTable[slotNdx] = NULL;                                                         \
                                                                                                                    \
                        lastSlot->nextSlot = set->slotFreeList;                                                     \
                        set->slotFreeList  = lastSlot;                                                              \
                    }                                                                                               \
                                                                                                                    \
                    set->numElements--;                                                                             \
                    return;                                                                                         \
                }                                                                                                   \
            }                                                                                                       \
                                                                                                                    \
            prevSlot = slot;                                                                                        \
            slot     = slot->nextSlot;                                                                              \
            DE_ASSERT(slot);                                                                                        \
        }                                                                                                           \
    }                                                                                                               \
                                                                                                                    \
    struct TYPENAME##Unused2_s                                                                                      \
    {                                                                                                               \
        int unused;                                                                                                 \
    }

/* Copy-to-array templates. */

#define DE_DECLARE_POOL_SET_TO_ARRAY(SETTYPENAME, ARRAYTYPENAME)                              \
    bool SETTYPENAME##_copyToArray(const SETTYPENAME *set, DE_PTR_TYPE(ARRAYTYPENAME) array); \
    struct SETTYPENAME##_##ARRAYTYPENAME##_declare_unused                                     \
    {                                                                                         \
        int unused;                                                                           \
    }

#define DE_IMPLEMENT_POOL_SET_TO_ARRAY(SETTYPENAME, ARRAYTYPENAME)                           \
    bool SETTYPENAME##_copyToArray(const SETTYPENAME *set, DE_PTR_TYPE(ARRAYTYPENAME) array) \
    {                                                                                        \
        int numElements = set->numElements;                                                  \
        int arrayNdx    = 0;                                                                 \
        int slotNdx;                                                                         \
                                                                                             \
        if (!ARRAYTYPENAME##_setSize(array, numElements))                                    \
            return false;                                                                    \
                                                                                             \
        for (slotNdx = 0; slotNdx < set->slotTableSize; slotNdx++)                           \
        {                                                                                    \
            const SETTYPENAME##Slot *slot = set->slotTable[slotNdx];                         \
            while (slot)                                                                     \
            {                                                                                \
                int elemNdx;                                                                 \
                for (elemNdx = 0; elemNdx < slot->numUsed; elemNdx++)                        \
                    ARRAYTYPENAME##_set(array, arrayNdx++, slot->keys[elemNdx]);             \
                slot = slot->nextSlot;                                                       \
            }                                                                                \
        }                                                                                    \
        DE_ASSERT(arrayNdx == numElements);                                                  \
        return true;                                                                         \
    }                                                                                        \
    struct SETTYPENAME##_##ARRAYTYPENAME##_implement_unused                                  \
    {                                                                                        \
        int unused;                                                                          \
    }

/*--------------------------------------------------------------------*//*!
 * \brief Declare set-wise operations for a set template.
 * \param TYPENAME    Type name of the declared set.
 * \param KEYTYPE    Type of the key.
 *
 * This macro declares union and intersection operations for a set.
 * For implementation see DE_IMPLEMENT_POOL_SET_UNION_INTERSECT.
 *
 * \todo [petri] Detailed description.
 *
 * The functions for operating the set are:
 * \todo [petri] Figure out how to comment these in Doxygen-style.
 *
 * \code
 * bool    Set_union                (Set* to, const Set* a, const Set* b);
 * bool    Set_unionInplace        (Set* a, const Set* b);
 * bool    Set_intersect            (Set* to, const Set* a, const Set* b);
 * void        Set_intersectInplace    (Set* a, const Set* b);
 * bool   Set_difference            (Set* to, const Set* a, const Set* b);
 * void     Set_differenceInplace    (Set* a, const Set* b);
 * \endcode
*//*--------------------------------------------------------------------*/
#define DE_DECLARE_POOL_SET_SETWISE_OPERATIONS(TYPENAME)                                        \
    bool TYPENAME##_union(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b);      \
    bool TYPENAME##_unionInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b);                   \
    bool TYPENAME##_intersect(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b);  \
    void TYPENAME##_intersectInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b);               \
    bool TYPENAME##_difference(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b); \
    void TYPENAME##_differenceInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b);              \
    struct TYPENAME##SetwiseDeclareUnused_s                                                     \
    {                                                                                           \
        int unused;                                                                             \
    }

#define DE_IMPLEMENT_POOL_SET_SETWISE_OPERATIONS(TYPENAME, KEYTYPE)                                    \
    bool TYPENAME##_union(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b)              \
    {                                                                                                  \
        TYPENAME##_reset(to);                                                                          \
        if (!TYPENAME##_unionInplace(to, a))                                                           \
            return false;                                                                              \
        if (!TYPENAME##_unionInplace(to, b))                                                           \
            return false;                                                                              \
        return true;                                                                                   \
    }                                                                                                  \
                                                                                                       \
    bool TYPENAME##_unionInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b)                           \
    {                                                                                                  \
        TYPENAME##Iter iter;                                                                           \
        for (TYPENAME##Iter_init(b, &iter); TYPENAME##Iter_hasItem(&iter); TYPENAME##Iter_next(&iter)) \
        {                                                                                              \
            KEYTYPE key = TYPENAME##Iter_getKey(&iter);                                                \
            if (!TYPENAME##_exists(a, key))                                                            \
            {                                                                                          \
                if (!TYPENAME##_insert(a, key))                                                        \
                    return false;                                                                      \
            }                                                                                          \
        }                                                                                              \
        return true;                                                                                   \
    }                                                                                                  \
                                                                                                       \
    bool TYPENAME##_intersect(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b)          \
    {                                                                                                  \
        TYPENAME##Iter iter;                                                                           \
        TYPENAME##_reset(to);                                                                          \
        for (TYPENAME##Iter_init(a, &iter); TYPENAME##Iter_hasItem(&iter); TYPENAME##Iter_next(&iter)) \
        {                                                                                              \
            KEYTYPE key = TYPENAME##Iter_getKey(&iter);                                                \
            if (TYPENAME##_exists(b, key))                                                             \
            {                                                                                          \
                if (!TYPENAME##_insert(to, key))                                                       \
                    return false;                                                                      \
            }                                                                                          \
        }                                                                                              \
        return true;                                                                                   \
    }                                                                                                  \
                                                                                                       \
    void TYPENAME##_intersectInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b)                       \
    {                                                                                                  \
        DE_UNREF(a &&b);                                                                               \
        DE_FATAL("Not implemented.");                                                                  \
    }                                                                                                  \
                                                                                                       \
    bool TYPENAME##_difference(DE_PTR_TYPE(TYPENAME) to, const TYPENAME *a, const TYPENAME *b)         \
    {                                                                                                  \
        TYPENAME##Iter iter;                                                                           \
        TYPENAME##_reset(to);                                                                          \
        for (TYPENAME##Iter_init(a, &iter); TYPENAME##Iter_hasItem(&iter); TYPENAME##Iter_next(&iter)) \
        {                                                                                              \
            KEYTYPE key = TYPENAME##Iter_getKey(&iter);                                                \
            if (!TYPENAME##_exists(b, key))                                                            \
            {                                                                                          \
                if (!TYPENAME##_insert(to, key))                                                       \
                    return false;                                                                      \
            }                                                                                          \
        }                                                                                              \
        return true;                                                                                   \
    }                                                                                                  \
                                                                                                       \
    void TYPENAME##_differenceInplace(DE_PTR_TYPE(TYPENAME) a, const TYPENAME *b)                      \
    {                                                                                                  \
        TYPENAME##Iter iter;                                                                           \
        for (TYPENAME##Iter_init(b, &iter); TYPENAME##Iter_hasItem(&iter); TYPENAME##Iter_next(&iter)) \
        {                                                                                              \
            KEYTYPE key = TYPENAME##Iter_getKey(&iter);                                                \
            if (TYPENAME##_exists(a, key))                                                             \
                TYPENAME##_delete(a, key);                                                             \
        }                                                                                              \
    }                                                                                                  \
                                                                                                       \
    struct TYPENAME##UnionIntersectImplementUnused_s                                                   \
    {                                                                                                  \
        int unused;                                                                                    \
    }

#endif /* _DEPOOLSET_H */
