/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "CPUCount.h"
#include "Interpreter.h"
#include <assert.h>
#include <cstddef>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <vector>

#include "mbmalloc.h"

Interpreter::Interpreter(const char* fileName, bool shouldFreeAllObjects, bool useThreadId)
    : m_shouldFreeAllObjects(shouldFreeAllObjects)
    , m_useThreadId(useThreadId)
    , m_currentThreadId(0)
    , m_ops(1024)
{
    m_fd = open(fileName, O_RDONLY);
    if (m_fd == -1) {
        fprintf(stderr, "Failed to open op file %s: ", fileName);
        perror("");
        exit(-1);
    }

    struct stat buf;
    fstat(m_fd, &buf);

    m_opCount = buf.st_size / sizeof(Op);
    assert(m_opCount * sizeof(Op) == buf.st_size);

    size_t maxSlot = 0;

    std::vector<Op> ops(1024);
    size_t remaining = m_opCount * sizeof(Op);
    while (remaining) {
        size_t bytes = std::min(remaining, ops.size() * sizeof(Op));
        remaining -= bytes;
        read(m_fd, ops.data(), bytes);

        size_t opCount = bytes / sizeof(Op);
        for (size_t i = 0; i < opCount; ++i) {
            Op op = ops[i];
            if (op.slot > maxSlot)
                maxSlot = op.slot;
        }
    }

    m_objects.resize(maxSlot + 1);
}

Interpreter::~Interpreter()
{
    int result = close(m_fd);
    if (result == -1) {
        perror("Failed to close op file");
        exit(-1);
    }
}

void Interpreter::run()
{
    std::vector<Op> ops(1024);
    lseek(m_fd, 0, SEEK_SET);

    m_remaining = m_opCount * sizeof(Op);
    m_opsCursor = m_opsInBuffer = 0;
    doOnSameThread(0);

    for (auto thread : m_threads)
        thread->stop();

    for (auto thread : m_threads)
        delete thread;

    // A recording might not free all of its allocations.
    if (!m_shouldFreeAllObjects)
        return;

    for (size_t i = 0; i < m_objects.size(); ++i) {
        if (!m_objects[i].object)
            continue;
        mbfree(m_objects[i].object, m_objects[i].size);
        m_objects[i] = { 0, 0 };
    }
}

bool Interpreter::readOps()
{
    if (!m_remaining)
        return false;

    size_t bytes = std::min(m_remaining, m_ops.size() * sizeof(Op));
    m_remaining -= bytes;
    read(m_fd, m_ops.data(), bytes);
    m_opsCursor = 0;
    m_opsInBuffer = bytes / sizeof(Op);
    
    if (!m_opsInBuffer)
        return false;

    return true;
}

void Interpreter::doOnSameThread(ThreadId runThreadId)
{
    while (true) {
        if ((m_opsCursor >= m_opsInBuffer) && (!readOps())) {
            if (runThreadId)
                switchToThread(0);
            return;
        }

        for (; m_opsCursor < m_opsInBuffer; ++m_opsCursor) {
            Op op = m_ops[m_opsCursor];
            ThreadId threadId = op.threadId;
            if (m_useThreadId && (runThreadId != threadId)) {
                switchToThread(threadId);
                break;
            }

            doMallocOp(op, m_currentThreadId);
        }
    }
}

void Interpreter::switchToThread(ThreadId threadId)
{
    if (m_currentThreadId == threadId)
        return;

    for (ThreadId threadIndex = static_cast<ThreadId>(m_threads.size());
         threadIndex < threadId; ++threadIndex)
        m_threads.push_back(new Thread(this, threadId));

    ThreadId currentThreadId = m_currentThreadId;

    if (threadId == 0) {
        std::unique_lock<std::mutex> lock(m_threadMutex);
        m_currentThreadId = threadId;
        m_shouldRun.notify_one();
    } else
        m_threads[threadId - 1]->switchTo();
    
    if (currentThreadId == 0) {
        std::unique_lock<std::mutex> lock(m_threadMutex);
        m_shouldRun.wait(lock, [this](){return m_currentThreadId == 0; });
    } else
        m_threads[currentThreadId - 1]->waitToRun();
}

void Interpreter::detailedReport()
{
    size_t totalInUse = 0;
    size_t smallInUse = 0;
    size_t mediumInUse = 0;
    size_t largeInUse = 0;
    size_t extraLargeInUse = 0;
    size_t memoryAllocated = 0;

    for (size_t i = 0; i < m_objects.size(); ++i) {
        if (!m_objects[i].object)
            continue;
        size_t objectSize = m_objects[i].size;
        memoryAllocated += objectSize;
        totalInUse++;

        if (objectSize <= 256)
            smallInUse++;
        else if (objectSize <= 1024)
            mediumInUse++;
        else if (objectSize <= 1032192)
            largeInUse++;
        else
            extraLargeInUse++;
    }

    std::cout << "0B-256B objects in use: " << smallInUse << std::endl;
    std::cout << "257B-1K objects in use: " << mediumInUse << std::endl;
    std::cout << "  1K-1M objects in use: " << largeInUse << std::endl;
    std::cout << "    1M+ objects in use: " << extraLargeInUse << std::endl;
    std::cout << "  Total objects in use: " << totalInUse << std::endl;
    std::cout << "Total allocated memory: " << memoryAllocated / 1024 << "kB" << std::endl;
}
static size_t compute2toPower(unsigned log2n)
{
    // Check for bad alignment log2 value and return a bad alignment.
    if (log2n > 64)
        return 0xff00;

    size_t result = 1;
    while (log2n--)
        result <<= 1;
    
    return result;
}

void Interpreter::doMallocOp(Op op, ThreadId threadId)
{
    switch (op.opcode) {
        case op_malloc: {
            m_objects[op.slot] = { mbmalloc(op.size), op.size };
            assert(m_objects[op.slot].object);
            bzero(m_objects[op.slot].object, op.size);
            break;
        }
        case op_free: {
            if (!m_objects[op.slot].object)
                return;
            mbfree(m_objects[op.slot].object, m_objects[op.slot].size);
            m_objects[op.slot] = { 0, 0 };
            break;
        }
        case op_realloc: {
            if (!m_objects[op.slot].object)
                return;
            m_objects[op.slot] = { mbrealloc(m_objects[op.slot].object, m_objects[op.slot].size, op.size), op.size };
            break;
        }
        case op_align_malloc: {
            size_t alignment = compute2toPower(op.alignLog2);
            m_objects[op.slot] = { mbmemalign(alignment, op.size), op.size };
            assert(m_objects[op.slot].object);
            bzero(m_objects[op.slot].object, op.size);
            break;
        }
        default: {
            fprintf(stderr, "bad opcode: %d\n", op.opcode);
            abort();
            break;
        }
    }
}

Interpreter::Thread::Thread(Interpreter* myInterpreter, ThreadId threadId)
    : m_threadId(threadId)
    , m_myInterpreter(myInterpreter)
{
    m_thread = std::thread(&Thread::runThread, this);
}

void Interpreter::Thread::stop()
{
    m_myInterpreter->switchToThread(m_threadId);
}

Interpreter::Thread::~Thread()
{
    switchTo();
    m_thread.join();
}

void Interpreter::Thread::runThread()
{
    waitToRun();
    m_myInterpreter->doOnSameThread(m_threadId);
}

void Interpreter::Thread::waitToRun()
{
    std::unique_lock<std::mutex> lock(m_myInterpreter->m_threadMutex);
    m_shouldRun.wait(lock, [this](){return m_myInterpreter->m_currentThreadId == m_threadId; });
}

void Interpreter::Thread::switchTo()
{
    std::unique_lock<std::mutex> lock(m_myInterpreter->m_threadMutex);
    m_myInterpreter->m_currentThreadId = m_threadId;
    m_shouldRun.notify_one();
}
