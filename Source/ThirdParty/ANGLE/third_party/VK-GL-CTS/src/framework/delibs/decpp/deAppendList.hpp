#ifndef _DEAPPENDLIST_HPP
#define _DEAPPENDLIST_HPP
/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
 *
 * Copyright 2015 The Android Open Source Project
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
 * \brief Fast ordered append-only container
 *//*--------------------------------------------------------------------*/

#include "deDefs.hpp"
#include "deAtomic.h"
#include "deThread.h"
#include "deMemory.h"
#include "deInt32.h"

namespace de
{

/*--------------------------------------------------------------------*//*!
 * \brief Fast ordered append-only container
 *
 * AppendList provides data structure for recording ordered list of elements
 * quickly, while still providing good sequential read access speed.
 * It is good for example logging.
 *
 * AppendList allocates memory in blocks of blockSize elements. Choosing
 * too small blockSize will affect performance.
 *
 * Elements can be appended from multiple threads simultaneously but if
 * current block runs out, allocation of next block will happen in a single
 * thread and block others from inserting further elements until completed.
 * For that reason shared AppendList should not be used if there is a lot
 * of contention and instead per-thread AppendList's are recommended.
 *//*--------------------------------------------------------------------*/
template <typename ElementType>
class AppendList
{
public:
    AppendList(size_t blockSize);
    ~AppendList(void);

    void append(const ElementType &value);

    size_t size(void) const
    {
        return m_numElements;
    }

    void clear(void);

private:
    AppendList(const AppendList<ElementType> &);
    AppendList<ElementType> &operator=(const AppendList<ElementType> &);

    struct Block
    {
        const size_t blockNdx;
        ElementType *elements;
        Block *volatile next;

        Block(size_t blockNdx_, size_t size)
            : blockNdx(blockNdx_)
            , elements(reinterpret_cast<ElementType *>(deAlignedMalloc(
                  sizeof(ElementType) * size, deAlign32((uint32_t)alignOf<ElementType>(), (uint32_t)sizeof(void *)))))
            , next(nullptr)
        {
        }

        ~Block(void)
        {
            deAlignedFree(reinterpret_cast<void *>(elements));
        }
    };

    const size_t m_blockSize;
    volatile size_t m_numElements;
    Block *m_first;
    Block *volatile m_last;

public:
    template <typename CompatibleType>
    class Iterator
    {
    public:
        Iterator(Block *curBlock_, size_t blockSize_, size_t slotNdx_)
            : m_curBlock(curBlock_)
            , m_blockSize(blockSize_)
            , m_slotNdx(slotNdx_)
        {
        }

        bool operator!=(const Iterator<CompatibleType> &other) const
        {
            return m_curBlock != other.m_curBlock || m_slotNdx != other.m_slotNdx;
        }
        bool operator==(const Iterator<CompatibleType> &other) const
        {
            return m_curBlock == other.m_curBlock && m_slotNdx == other.m_slotNdx;
        }

        Iterator<CompatibleType> &operator++(void)
        {
            ++m_slotNdx;

            if (m_slotNdx == m_blockSize)
            {
                m_slotNdx  = 0;
                m_curBlock = m_curBlock->next;
            }

            return *this;
        }

        Iterator<CompatibleType> operator++(int) const
        {
            Iterator<CompatibleType> copy(*this);
            return ++copy;
        }

        CompatibleType &operator*(void) const
        {
            return m_curBlock->elements[m_slotNdx];
        }

        CompatibleType *operator->(void) const
        {
            return &m_curBlock->elements[m_slotNdx];
        }

        operator Iterator<const CompatibleType>(void) const
        {
            return Iterator<const CompatibleType>(m_curBlock, m_blockSize, m_slotNdx);
        }

    private:
        Block *m_curBlock;
        size_t m_blockSize;
        size_t m_slotNdx;
    };

    typedef Iterator<const ElementType> const_iterator;
    typedef Iterator<ElementType> iterator;

    const_iterator begin(void) const;
    iterator begin(void);

    const_iterator end(void) const;
    iterator end(void);
};

template <typename ElementType>
AppendList<ElementType>::AppendList(size_t blockSize)
    : m_blockSize(blockSize)
    , m_numElements(0)
    , m_first(new Block(0, blockSize))
    , m_last(m_first)
{
}

template <typename ElementType>
AppendList<ElementType>::~AppendList(void)
{
    size_t elementNdx = 0;
    Block *curBlock   = m_first;

    while (curBlock)
    {
        Block *const delBlock = curBlock;

        curBlock = delBlock->next;

        // Call destructor for allocated elements
        for (; elementNdx < min(m_numElements, (delBlock->blockNdx + 1) * m_blockSize); ++elementNdx)
            delBlock->elements[elementNdx % m_blockSize].~ElementType();

        delete delBlock;
    }

    DE_ASSERT(elementNdx == m_numElements);
}

template <typename ElementType>
void AppendList<ElementType>::clear(void)
{
    // \todo [2016-03-28 pyry] Make thread-safe, if possible

    size_t elementNdx = 0;
    Block *curBlock   = m_first;

    while (curBlock)
    {
        Block *const delBlock = curBlock;

        curBlock = delBlock->next;

        // Call destructor for allocated elements
        for (; elementNdx < min(m_numElements, (delBlock->blockNdx + 1) * m_blockSize); ++elementNdx)
            delBlock->elements[elementNdx % m_blockSize].~ElementType();

        if (delBlock != m_first)
            delete delBlock;
    }

    DE_ASSERT(elementNdx == m_numElements);

    m_numElements = 0;
    m_first->next = nullptr;
    m_last        = m_first;
}

template <typename ElementType>
void AppendList<ElementType>::append(const ElementType &value)
{
    // Fetch curBlock first before allocating slot. Otherwise m_last might get updated before
    // this thread gets chance of reading it, leading to curBlock->blockNdx > blockNdx.
    Block *curBlock = m_last;

    deMemoryReadWriteFence();

    {
        const size_t elementNdx = deAtomicIncrementUSize(&m_numElements) - 1;
        const size_t blockNdx   = elementNdx / m_blockSize;
        const size_t slotNdx    = elementNdx - (blockNdx * m_blockSize);

        while (curBlock->blockNdx != blockNdx)
        {
            if (curBlock->next)
                curBlock = curBlock->next;
            else
            {
                // Other thread(s) are currently allocating additional block(s)
                deYield();
            }
        }

        // Did we allocate last slot? If so, add a new block
        if (slotNdx + 1 == m_blockSize)
        {
            Block *const newBlock = new Block(blockNdx + 1, m_blockSize);

            deMemoryReadWriteFence();

            // At this point if any other thread is trying to allocate more blocks
            // they are being blocked by curBlock->next being null. This guarantees
            // that this thread has exclusive modify access to m_last.
            m_last = newBlock;
            deMemoryReadWriteFence();

            // At this point other threads might have skipped to newBlock, but we
            // still have exclusive modify access to curBlock->next.
            curBlock->next = newBlock;
            deMemoryReadWriteFence();
        }

        new (&curBlock->elements[slotNdx]) ElementType(value);
    }
}

template <typename ElementType>
typename AppendList<ElementType>::const_iterator AppendList<ElementType>::begin(void) const
{
    return const_iterator(m_first, m_blockSize, 0);
}

template <typename ElementType>
typename AppendList<ElementType>::iterator AppendList<ElementType>::begin(void)
{
    return iterator(m_first, m_blockSize, 0);
}

template <typename ElementType>
typename AppendList<ElementType>::const_iterator AppendList<ElementType>::end(void) const
{
    return const_iterator(m_last, m_blockSize, m_numElements % m_blockSize);
}

template <typename ElementType>
typename AppendList<ElementType>::iterator AppendList<ElementType>::end(void)
{
    return iterator(m_last, m_blockSize, m_numElements % m_blockSize);
}

void AppendList_selfTest(void);

} // namespace de

#endif // _DEAPPENDLIST_HPP
