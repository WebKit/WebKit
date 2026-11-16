#ifndef _DEATOMIC_H
#define _DEATOMIC_H
/*-------------------------------------------------------------------------
 * drawElements Thread Library
 * ---------------------------
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
 * \brief Atomic operations.
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

#if (DE_COMPILER == DE_COMPILER_MSC)
#include <intrin.h>
#endif

DE_BEGIN_EXTERN_C

/*--------------------------------------------------------------------*//*!
 * \brief Atomic increment and fetch 32-bit signed integer.
 * \param dstAddr    Destination address.
 * \return Incremented value.
 *//*--------------------------------------------------------------------*/
static inline int32_t deAtomicIncrementInt32(volatile int32_t *dstAddr)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
    return _InterlockedIncrement((long volatile *)dstAddr);
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
    return __sync_add_and_fetch(dstAddr, 1);
#else
#error "Implement deAtomicIncrementInt32()"
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Atomic increment and fetch 32-bit unsigned integer.
 * \param dstAddr    Destination address.
 * \return Incremented value.
 *//*--------------------------------------------------------------------*/
static inline uint32_t deAtomicIncrementUint32(volatile uint32_t *dstAddr)
{
    return deAtomicIncrementInt32((int32_t volatile *)dstAddr);
}

/*--------------------------------------------------------------------*//*!
 * \brief Atomic decrement and fetch 32-bit signed integer.
 * \param dstAddr    Destination address.
 * \return Decremented value.
 *//*--------------------------------------------------------------------*/
static inline int32_t deAtomicDecrementInt32(volatile int32_t *dstAddr)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
    return _InterlockedDecrement((volatile long *)dstAddr);
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
    return __sync_sub_and_fetch(dstAddr, 1);
#else
#error "Implement deAtomicDecrementInt32()"
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Atomic decrement and fetch 32-bit unsigned integer.
 * \param dstAddr    Destination address.
 * \return Decremented value.
 *//*--------------------------------------------------------------------*/
static inline uint32_t deAtomicDecrementUint32(volatile uint32_t *dstAddr)
{
    return deAtomicDecrementInt32((volatile int32_t *)dstAddr);
}

/*--------------------------------------------------------------------*//*!
 * \brief Atomic compare and exchange (CAS) 32-bit value.
 * \param dstAddr    Destination address.
 * \param compare    Old value.
 * \param exchange    New value.
 * \return            compare value if CAS passes, *dstAddr value otherwise
 *
 * Performs standard Compare-And-Swap with 32b data. Dst value is compared
 * to compare value and if that comparison passes, value is replaced with
 * exchange value.
 *
 * If CAS succeeds, compare value is returned. Otherwise value stored in
 * dstAddr is returned.
 *//*--------------------------------------------------------------------*/
static inline uint32_t deAtomicCompareExchangeUint32(volatile uint32_t *dstAddr, uint32_t compare, uint32_t exchange)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
    return _InterlockedCompareExchange((volatile long *)dstAddr, exchange, compare);
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
    return __sync_val_compare_and_swap(dstAddr, compare, exchange);
#else
#error "Implement deAtomicCompareExchange32()"
#endif
}

/* Deprecated names */
#define deAtomicIncrement32 deAtomicIncrementInt32
#define deAtomicDecrement32 deAtomicDecrementInt32
#define deAtomicCompareExchange32 deAtomicCompareExchangeUint32

#if (DE_PTR_SIZE == 8)

/*--------------------------------------------------------------------*//*!
 * \brief Atomic increment and fetch 64-bit signed integer.
 * \param dstAddr    Destination address.
 * \return Incremented value.
 *//*--------------------------------------------------------------------*/
static inline int64_t deAtomicIncrementInt64(volatile int64_t *dstAddr)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
    return _InterlockedIncrement64((volatile long long *)dstAddr);
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
    return __sync_add_and_fetch(dstAddr, 1);
#else
#error "Implement deAtomicIncrementInt64()"
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Atomic increment and fetch 64-bit unsigned integer.
 * \param dstAddr    Destination address.
 * \return Incremented value.
 *//*--------------------------------------------------------------------*/
static inline uint64_t deAtomicIncrementUint64(volatile uint64_t *dstAddr)
{
    return deAtomicIncrementInt64((volatile int64_t *)dstAddr);
}

/*--------------------------------------------------------------------*//*!
 * \brief Atomic decrement and fetch.
 * \param dstAddr    Destination address.
 * \return Decremented value.
 *//*--------------------------------------------------------------------*/
static inline int64_t deAtomicDecrementInt64(volatile int64_t *dstAddr)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
    return _InterlockedDecrement64((volatile long long *)dstAddr);
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
    return __sync_sub_and_fetch(dstAddr, 1);
#else
#error "Implement deAtomicDecrementInt64()"
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Atomic increment and fetch 64-bit unsigned integer.
 * \param dstAddr    Destination address.
 * \return Incremented value.
 *//*--------------------------------------------------------------------*/
static inline uint64_t deAtomicDecrementUint64(volatile uint64_t *dstAddr)
{
    return deAtomicDecrementInt64((volatile int64_t *)dstAddr);
}

/*--------------------------------------------------------------------*//*!
 * \brief Atomic compare and exchange (CAS) 64-bit value.
 * \param dstAddr    Destination address.
 * \param compare    Old value.
 * \param exchange    New value.
 * \return            compare value if CAS passes, *dstAddr value otherwise
 *
 * Performs standard Compare-And-Swap with 64b data. Dst value is compared
 * to compare value and if that comparison passes, value is replaced with
 * exchange value.
 *
 * If CAS succeeds, compare value is returned. Otherwise value stored in
 * dstAddr is returned.
 *//*--------------------------------------------------------------------*/
static inline uint64_t deAtomicCompareExchangeUint64(volatile uint64_t *dstAddr, uint64_t compare, uint64_t exchange)
{
#if (DE_COMPILER == DE_COMPILER_MSC)
    return _InterlockedCompareExchange64((volatile long long *)dstAddr, exchange, compare);
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
    return __sync_val_compare_and_swap(dstAddr, compare, exchange);
#else
#error "Implement deAtomicCompareExchangeUint64()"
#endif
}

#endif /* (DE_PTR_SIZE == 8) */

/*--------------------------------------------------------------------*//*!
 * \brief Atomic increment and fetch size_t.
 * \param dstAddr    Destination address.
 * \return Incremented value.
 *//*--------------------------------------------------------------------*/
static inline size_t deAtomicIncrementUSize(volatile size_t *size)
{
#if (DE_PTR_SIZE == 8)
    return deAtomicIncrementUint64((volatile uint64_t *)size);
#elif (DE_PTR_SIZE == 4)
    return deAtomicIncrementUint32((volatile uint32_t *)size);
#else
#error "Invalid DE_PTR_SIZE value"
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Atomic increment and fetch size_t.
 * \param dstAddr    Destination address.
 * \return Incremented value.
 *//*--------------------------------------------------------------------*/
static inline size_t deAtomicDecrementUSize(volatile size_t *size)
{
#if (DE_PTR_SIZE == 8)
    return deAtomicDecrementUint64((volatile uint64_t *)size);
#elif (DE_PTR_SIZE == 4)
    return deAtomicDecrementUint32((volatile uint32_t *)size);
#else
#error "Invalid DE_PTR_SIZE value"
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Atomic compare and exchange (CAS) pointer.
 * \param dstAddr    Destination address.
 * \param compare    Old value.
 * \param exchange    New value.
 * \return            compare value if CAS passes, *dstAddr value otherwise
 *
 * Performs standard Compare-And-Swap with pointer value. Dst value is compared
 * to compare value and if that comparison passes, value is replaced with
 * exchange value.
 *
 * If CAS succeeds, compare value is returned. Otherwise value stored in
 * dstAddr is returned.
 *//*--------------------------------------------------------------------*/
static inline void *deAtomicCompareExchangePtr(void *volatile *dstAddr, void *compare, void *exchange)
{
#if (DE_PTR_SIZE == 8)
    return (void *)deAtomicCompareExchangeUint64((volatile uint64_t *)dstAddr, (uint64_t)compare, (uint64_t)exchange);
#elif (DE_PTR_SIZE == 4)
    return (void *)deAtomicCompareExchangeUint32((volatile uint32_t *)dstAddr, (uint32_t)compare, (uint32_t)exchange);
#else
#error "Invalid DE_PTR_SIZE value"
#endif
}

/*--------------------------------------------------------------------*//*!
 * \brief Issue hardware memory read-write fence.
 *//*--------------------------------------------------------------------*/
#if (DE_COMPILER == DE_COMPILER_MSC)
void deMemoryReadWriteFence(void);
#elif (DE_COMPILER == DE_COMPILER_GCC) || (DE_COMPILER == DE_COMPILER_CLANG)
static inline void deMemoryReadWriteFence(void)
{
    __sync_synchronize();
}
#else
#error "Implement deMemoryReadWriteFence()"
#endif

DE_END_EXTERN_C

#endif /* _DEATOMIC_H */
