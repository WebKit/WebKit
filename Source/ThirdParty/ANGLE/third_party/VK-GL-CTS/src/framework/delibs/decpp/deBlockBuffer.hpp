#ifndef _DEBLOCKBUFFER_HPP
#define _DEBLOCKBUFFER_HPP
/*-------------------------------------------------------------------------
 * drawElements C++ Base Library
 * -----------------------------
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
 * \brief Block-based thread-safe queue.
 *//*--------------------------------------------------------------------*/

#include "deBlockBuffer.hpp"
#include "deMutex.hpp"
#include "deSemaphore.h"

#include <exception>

namespace de
{

void BlockBuffer_selfTest(void);

class BufferCanceledException : public std::exception
{
public:
    inline BufferCanceledException(void)
    {
    }
    inline ~BufferCanceledException(void) throw()
    {
    }

    const char *what(void) const throw()
    {
        return "BufferCanceledException";
    }
};

template <typename T>
class BlockBuffer
{
public:
    typedef BufferCanceledException CanceledException;

    BlockBuffer(int blockSize, int numBlocks);
    ~BlockBuffer(void);

    void clear(void); //!< Resets buffer. Will block until pending writes and reads have completed.

    void write(int numElements, const T *elements);
    int tryWrite(int numElements, const T *elements);
    void flush(void);
    bool tryFlush(void);

    void read(int numElements, T *elements);
    int tryRead(int numElements, T *elements);

    void cancel(
        void); //!< Sets buffer in canceled state. All (including pending) writes and reads will result in CanceledException.
    bool isCanceled(void) const
    {
        return !!m_canceled;
    }

private:
    BlockBuffer(const BlockBuffer &other);
    BlockBuffer &operator=(const BlockBuffer &other);

    int writeToCurrentBlock(int numElements, const T *elements, bool blocking);
    int readFromCurrentBlock(int numElements, T *elements, bool blocking);

    void flushWriteBlock(void);

    deSemaphore m_fill;  //!< Block fill count.
    deSemaphore m_empty; //!< Block empty count.

    int m_writeBlock; //!< Current write block ndx.
    int m_writePos;   //!< Position in block. 0 if block is not yet acquired.

    int m_readBlock; //!< Current read block ndx.
    int m_readPos;   //!< Position in block. 0 if block is not yet acquired.

    int m_blockSize;
    int m_numBlocks;

    T *m_elements;
    int *m_numUsedInBlock;

    Mutex m_writeLock;
    Mutex m_readLock;

    volatile uint32_t m_canceled;
} DE_WARN_UNUSED_TYPE;

template <typename T>
BlockBuffer<T>::BlockBuffer(int blockSize, int numBlocks)
    : m_fill(0)
    , m_empty(0)
    , m_writeBlock(0)
    , m_writePos(0)
    , m_readBlock(0)
    , m_readPos(0)
    , m_blockSize(blockSize)
    , m_numBlocks(numBlocks)
    , m_elements(nullptr)
    , m_numUsedInBlock(nullptr)
    , m_writeLock()
    , m_readLock()
    , m_canceled(false)
{
    DE_ASSERT(blockSize > 0);
    DE_ASSERT(numBlocks > 0);

    try
    {
        m_elements       = new T[m_numBlocks * m_blockSize];
        m_numUsedInBlock = new int[m_numBlocks];
    }
    catch (...)
    {
        delete[] m_elements;
        delete[] m_numUsedInBlock;
        throw;
    }

    m_fill  = deSemaphore_create(0, nullptr);
    m_empty = deSemaphore_create(numBlocks, nullptr);
    DE_ASSERT(m_fill && m_empty);
}

template <typename T>
BlockBuffer<T>::~BlockBuffer(void)
{
    delete[] m_elements;
    delete[] m_numUsedInBlock;

    deSemaphore_destroy(m_fill);
    deSemaphore_destroy(m_empty);
}

template <typename T>
void BlockBuffer<T>::clear(void)
{
    ScopedLock readLock(m_readLock);
    ScopedLock writeLock(m_writeLock);

    deSemaphore_destroy(m_fill);
    deSemaphore_destroy(m_empty);

    m_fill       = deSemaphore_create(0, nullptr);
    m_empty      = deSemaphore_create(m_numBlocks, nullptr);
    m_writeBlock = 0;
    m_writePos   = 0;
    m_readBlock  = 0;
    m_readPos    = 0;
    m_canceled   = false;

    DE_ASSERT(m_fill && m_empty);
}

template <typename T>
void BlockBuffer<T>::cancel(void)
{
    DE_ASSERT(!m_canceled);
    m_canceled = true;

    deSemaphore_increment(m_empty);
    deSemaphore_increment(m_fill);
}

template <typename T>
int BlockBuffer<T>::writeToCurrentBlock(int numElements, const T *elements, bool blocking)
{
    DE_ASSERT(numElements > 0 && elements != nullptr);

    if (m_writePos == 0)
    {
        /* Write thread doesn't own current block - need to acquire. */
        if (blocking)
            deSemaphore_decrement(m_empty);
        else
        {
            if (!deSemaphore_tryDecrement(m_empty))
                return 0;
        }

        /* Check for canceled bit. */
        if (m_canceled)
        {
            // \todo [2012-07-06 pyry] A bit hackish to assume that write lock is not freed if exception is thrown out here.
            deSemaphore_increment(m_empty);
            m_writeLock.unlock();
            throw CanceledException();
        }
    }

    /* Write thread owns current block. */
    T *block       = m_elements + m_writeBlock * m_blockSize;
    int numToWrite = de::min(numElements, m_blockSize - m_writePos);

    DE_ASSERT(numToWrite > 0);

    for (int ndx = 0; ndx < numToWrite; ndx++)
        block[m_writePos + ndx] = elements[ndx];

    m_writePos += numToWrite;

    if (m_writePos == m_blockSize)
        flushWriteBlock(); /* Flush current write block. */

    return numToWrite;
}

template <typename T>
int BlockBuffer<T>::readFromCurrentBlock(int numElements, T *elements, bool blocking)
{
    DE_ASSERT(numElements > 0 && elements != nullptr);

    if (m_readPos == 0)
    {
        /* Read thread doesn't own current block - need to acquire. */
        if (blocking)
            deSemaphore_decrement(m_fill);
        else
        {
            if (!deSemaphore_tryDecrement(m_fill))
                return 0;
        }

        /* Check for canceled bit. */
        if (m_canceled)
        {
            // \todo [2012-07-06 pyry] A bit hackish to assume that read lock is not freed if exception is thrown out here.
            deSemaphore_increment(m_fill);
            m_readLock.unlock();
            throw CanceledException();
        }
    }

    /* Read thread now owns current block. */
    const T *block     = m_elements + m_readBlock * m_blockSize;
    int numUsedInBlock = m_numUsedInBlock[m_readBlock];
    int numToRead      = de::min(numElements, numUsedInBlock - m_readPos);

    DE_ASSERT(numToRead > 0);

    for (int ndx = 0; ndx < numToRead; ndx++)
        elements[ndx] = block[m_readPos + ndx];

    m_readPos += numToRead;

    if (m_readPos == numUsedInBlock)
    {
        /* Free current read block and advance. */
        m_readBlock = (m_readBlock + 1) % m_numBlocks;
        m_readPos   = 0;
        deSemaphore_increment(m_empty);
    }

    return numToRead;
}

template <typename T>
int BlockBuffer<T>::tryWrite(int numElements, const T *elements)
{
    int numWritten = 0;

    DE_ASSERT(numElements > 0 && elements != nullptr);

    if (m_canceled)
        throw CanceledException();

    if (!m_writeLock.tryLock())
        return numWritten;

    while (numWritten < numElements)
    {
        int ret = writeToCurrentBlock(numElements - numWritten, elements + numWritten, false /* non-blocking */);

        if (ret == 0)
            break; /* Write failed. */

        numWritten += ret;
    }

    m_writeLock.unlock();

    return numWritten;
}

template <typename T>
void BlockBuffer<T>::write(int numElements, const T *elements)
{
    DE_ASSERT(numElements > 0 && elements != nullptr);

    if (m_canceled)
        throw CanceledException();

    m_writeLock.lock();

    int numWritten = 0;
    while (numWritten < numElements)
        numWritten += writeToCurrentBlock(numElements - numWritten, elements + numWritten, true /* blocking */);

    m_writeLock.unlock();
}

template <typename T>
void BlockBuffer<T>::flush(void)
{
    m_writeLock.lock();

    if (m_writePos > 0)
        flushWriteBlock();

    m_writeLock.unlock();
}

template <typename T>
bool BlockBuffer<T>::tryFlush(void)
{
    if (!m_writeLock.tryLock())
        return false;

    if (m_writePos > 0)
        flushWriteBlock();

    m_writeLock.unlock();

    return true;
}

template <typename T>
void BlockBuffer<T>::flushWriteBlock(void)
{
    DE_ASSERT(de::inRange(m_writePos, 1, m_blockSize));

    m_numUsedInBlock[m_writeBlock] = m_writePos;
    m_writeBlock                   = (m_writeBlock + 1) % m_numBlocks;
    m_writePos                     = 0;
    deSemaphore_increment(m_fill);
}

template <typename T>
int BlockBuffer<T>::tryRead(int numElements, T *elements)
{
    int numRead = 0;

    if (m_canceled)
        throw CanceledException();

    if (!m_readLock.tryLock())
        return numRead;

    while (numRead < numElements)
    {
        int ret = readFromCurrentBlock(numElements - numRead, &elements[numRead], false /* non-blocking */);

        if (ret == 0)
            break; /* Failed. */

        numRead += ret;
    }

    m_readLock.unlock();

    return numRead;
}

template <typename T>
void BlockBuffer<T>::read(int numElements, T *elements)
{
    DE_ASSERT(numElements > 0 && elements != nullptr);

    if (m_canceled)
        throw CanceledException();

    m_readLock.lock();

    int numRead = 0;
    while (numRead < numElements)
        numRead += readFromCurrentBlock(numElements - numRead, &elements[numRead], true /* blocking */);

    m_readLock.unlock();
}

} // namespace de

#endif // _DEBLOCKBUFFER_HPP
