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

#ifndef Interpreter_h
#define Interpreter_h

#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

class Interpreter {
public:
    Interpreter(const char* fileName, bool shouldFreeAllObjects = true, bool useThreadId = false);
    ~Interpreter();

    void run();
    void detailedReport();

private:
    typedef unsigned short ThreadId; // 0 is the main thread
    typedef unsigned short Log2Alignment; // log2(alignment) or ~0 for non power of 2.
    enum Opcode { op_malloc, op_free, op_realloc, op_align_malloc };
    struct Op { Opcode opcode; ThreadId threadId; Log2Alignment alignLog2; size_t slot; size_t size; };
    struct Record { void* object; size_t size; };

    class Thread
    {
    public:
        Thread(Interpreter*, ThreadId);
        ~Thread();

        void runThread();

        void waitToRun();
        void switchTo();
        void stop();
        
        bool isMainThread() { return m_threadId == 0; }

    private:
        ThreadId m_threadId;
        Interpreter* m_myInterpreter;
        std::condition_variable m_shouldRun;
        std::thread m_thread;
    };

    bool readOps();
    void doOnSameThread(ThreadId);
    void switchToThread(ThreadId);

    void doMallocOp(Op, ThreadId);
    
    bool m_shouldFreeAllObjects;
    bool m_useThreadId;
    int m_fd;
    size_t m_opCount;
    size_t m_remaining;
    size_t m_opsCursor;
    size_t m_opsInBuffer;
    ThreadId m_currentThreadId;
    std::vector<Op> m_ops;
    std::mutex m_threadMutex;
    std::condition_variable m_shouldRun;
    std::vector<Thread*> m_threads;
    std::vector<Record> m_objects;
};

#endif // Interpreter_h
