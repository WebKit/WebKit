/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RegisterFile_h
#define RegisterFile_h

#include "Collector.h"
#include "ExecutableAllocator.h"
#include "Register.h"
#include <wtf/Noncopyable.h>
#include <wtf/VMTags.h>

#if HAVE(MMAP)
#include <errno.h>
#include <stdio.h>
#include <sys/mman.h>
#endif

namespace JSC {

/*
    A register file is a stack of register frames. We represent a register
    frame by its offset from "base", the logical first entry in the register
    file. The bottom-most register frame's offset from base is 0.

    In a program where function "a" calls function "b" (global code -> a -> b),
    the register file might look like this:

    |       global frame     |        call frame      |        call frame      |     spare capacity     |
    -----------------------------------------------------------------------------------------------------
    |  0 |  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8 |  9 | 10 | 11 | 12 | 13 | 14 |    |    |    |    |    | <-- index in buffer
    -----------------------------------------------------------------------------------------------------
    | -3 | -2 | -1 |  0 |  1 |  2 |  3 |  4 |  5 |  6 |  7 |  8 |  9 | 10 | 11 |    |    |    |    |    | <-- index relative to base
    -----------------------------------------------------------------------------------------------------
    |    <-globals | temps-> |  <-vars | temps->      |                 <-vars |
       ^              ^                   ^                                       ^
       |              |                   |                                       |
     buffer    base (frame 0)          frame 1                                 frame 2

    Since all variables, including globals, are accessed by negative offsets
    from their register frame pointers, to keep old global offsets correct, new
    globals must appear at the beginning of the register file, shifting base
    to the right.

    If we added one global variable to the register file depicted above, it
    would look like this:

    |         global frame        |<                                                                    >
    ------------------------------->                                                                    <
    |  0 |  1 |  2 |  3 |  4 |  5 |<                             >snip<                                 > <-- index in buffer
    ------------------------------->                                                                    <
    | -4 | -3 | -2 | -1 |  0 |  1 |<                                                                    > <-- index relative to base
    ------------------------------->                                                                    <
    |         <-globals | temps-> |
       ^                   ^
       |                   |
     buffer         base (frame 0)

    As you can see, global offsets relative to base have stayed constant,
    but base itself has moved. To keep up with possible changes to base,
    clients keep an indirect pointer, so their calculations update
    automatically when base changes.

    For client simplicity, the RegisterFile measures size and capacity from
    "base", not "buffer".
*/

    class JSGlobalObject;

    class RegisterFile : Noncopyable {
        friend class JIT;
    public:
        enum CallFrameHeaderEntry {
            CallFrameHeaderSize = 8,

            CodeBlock = -8,
            ScopeChain = -7,
            CallerFrame = -6,
            ReturnPC = -5, // This is either an Instruction* or a pointer into JIT generated code stored as an Instruction*.
            ReturnValueRegister = -4,
            ArgumentCount = -3,
            Callee = -2,
            OptionalCalleeArguments = -1,
        };

        enum { ProgramCodeThisRegister = -CallFrameHeaderSize - 1 };
        enum { ArgumentsRegister = 0 };

        static const size_t defaultCapacity = 524288;
        static const size_t defaultMaxGlobals = 8192;
        static const size_t commitSize = 1 << 14;
        // Allow 8k of excess registers before we start trying to reap the registerfile
        static const ptrdiff_t maxExcessCapacity = 8 * 1024;

        RegisterFile(size_t capacity = defaultCapacity, size_t maxGlobals = defaultMaxGlobals);
        ~RegisterFile();

        Register* start() const { return m_start; }
        Register* end() const { return m_end; }
        size_t size() const { return m_end - m_start; }

        void setGlobalObject(JSGlobalObject* globalObject) { m_globalObject = globalObject; }
        JSGlobalObject* globalObject() { return m_globalObject; }

        bool grow(Register* newEnd);
        void shrink(Register* newEnd);
        
        void setNumGlobals(size_t numGlobals) { m_numGlobals = numGlobals; }
        int numGlobals() const { return m_numGlobals; }
        size_t maxGlobals() const { return m_maxGlobals; }

        Register* lastGlobal() const { return m_start - m_numGlobals; }
        
        void markGlobals(Heap* heap) { heap->markConservatively(lastGlobal(), m_start); }
        void markCallFrames(Heap* heap) { heap->markConservatively(m_start, m_end); }

    private:
        void releaseExcessCapacity();
        size_t m_numGlobals;
        const size_t m_maxGlobals;
        Register* m_start;
        Register* m_end;
        Register* m_max;
        Register* m_buffer;
        Register* m_maxUsed;

#if HAVE(VIRTUALALLOC)
        Register* m_commitEnd;
#endif

        JSGlobalObject* m_globalObject; // The global object whose vars are currently stored in the register file.
    };

    // FIXME: Add a generic getpagesize() to WTF, then move this function to WTF as well.
    inline bool isPageAligned(size_t size) { return size != 0 && size % (8 * 1024) == 0; }

    inline RegisterFile::RegisterFile(size_t capacity, size_t maxGlobals)
        : m_numGlobals(0)
        , m_maxGlobals(maxGlobals)
        , m_start(0)
        , m_end(0)
        , m_max(0)
        , m_buffer(0)
        , m_globalObject(0)
    {
        // Verify that our values will play nice with mmap and VirtualAlloc.
        ASSERT(isPageAligned(maxGlobals));
        ASSERT(isPageAligned(capacity));

        size_t bufferLength = (capacity + maxGlobals) * sizeof(Register);
    #if HAVE(MMAP)
        m_buffer = static_cast<Register*>(mmap(0, bufferLength, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, VM_TAG_FOR_REGISTERFILE_MEMORY, 0));
        if (m_buffer == MAP_FAILED) {
            fprintf(stderr, "Could not allocate register file: %d\n", errno);
            CRASH();
        }
    #elif HAVE(VIRTUALALLOC)
        m_buffer = static_cast<Register*>(VirtualAlloc(0, roundUpAllocationSize(bufferLength, commitSize), MEM_RESERVE, PAGE_READWRITE));
        if (!m_buffer) {
            fprintf(stderr, "Could not allocate register file: %d\n", errno);
            CRASH();
        }
        size_t committedSize = roundUpAllocationSize(maxGlobals * sizeof(Register), commitSize);
        void* commitCheck = VirtualAlloc(m_buffer, committedSize, MEM_COMMIT, PAGE_READWRITE);
        if (commitCheck != m_buffer) {
            fprintf(stderr, "Could not allocate register file: %d\n", errno);
            CRASH();
        }
        m_commitEnd = reinterpret_cast<Register*>(reinterpret_cast<char*>(m_buffer) + committedSize);
    #else
        #error "Don't know how to reserve virtual memory on this platform."
    #endif
        m_start = m_buffer + maxGlobals;
        m_end = m_start;
        m_maxUsed = m_end;
        m_max = m_start + capacity;
    }

    inline void RegisterFile::shrink(Register* newEnd)
    {
        if (newEnd >= m_end)
            return;
        m_end = newEnd;
        if (m_end == m_start && (m_maxUsed - m_start) > maxExcessCapacity)
            releaseExcessCapacity();
    }

    inline bool RegisterFile::grow(Register* newEnd)
    {
        if (newEnd < m_end)
            return true;

        if (newEnd > m_max)
            return false;

#if !HAVE(MMAP) && HAVE(VIRTUALALLOC)
        if (newEnd > m_commitEnd) {
            size_t size = roundUpAllocationSize(reinterpret_cast<char*>(newEnd) - reinterpret_cast<char*>(m_commitEnd), commitSize);
            if (!VirtualAlloc(m_commitEnd, size, MEM_COMMIT, PAGE_READWRITE)) {
                fprintf(stderr, "Could not allocate register file: %d\n", errno);
                CRASH();
            }
            m_commitEnd = reinterpret_cast<Register*>(reinterpret_cast<char*>(m_commitEnd) + size);
        }
#endif

        if (newEnd > m_maxUsed)
            m_maxUsed = newEnd;

        m_end = newEnd;
        return true;
    }

} // namespace JSC

#endif // RegisterFile_h
