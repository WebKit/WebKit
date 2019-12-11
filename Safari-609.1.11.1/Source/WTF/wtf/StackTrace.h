/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Yusuke Suzuki <utatane.tea@gmail.com>
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

#pragma once

#include <wtf/Optional.h>
#include <wtf/SystemFree.h>

namespace WTF {

class PrintStream;

class StackTrace {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WTF_EXPORT_PRIVATE static std::unique_ptr<StackTrace> captureStackTrace(int maxFrames, int framesToSkip = 0);

    // Borrowed stack trace.
    StackTrace(void** stack, int size)
        : m_size(size)
        , m_capacity(0)
        , m_borrowedStack(stack)
    { }

    int size() const { return m_size; }
    void* const * stack() const
    {
        if (!m_capacity)
            return m_borrowedStack;
        return m_stack;
    }

    class DemangleEntry {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        friend class StackTrace;
        const char* mangledName() const { return m_mangledName; }
        const char* demangledName() const { return m_demangledName.get(); }

    private:
        DemangleEntry(const char* mangledName, const char* demangledName)
            : m_mangledName(mangledName)
            , m_demangledName(demangledName)
        { }

        const char* m_mangledName { nullptr };
        std::unique_ptr<const char[], SystemFree<const char[]>> m_demangledName;
    };

    WTF_EXPORT_PRIVATE static Optional<DemangleEntry> demangle(void*);

    WTF_EXPORT_PRIVATE void dump(PrintStream&, const char* indentString = nullptr) const;

private:
    inline static size_t instanceSize(int capacity);

    StackTrace()
        : m_size(0)
    { }

    // We structure the top fields this way because the underlying stack capture
    // facility will capture from the top of the stack, and we'll need to skip the
    // top 2 frame which is of no interest. Setting up the fields layout this way
    // allows us to capture the stack in place and minimize space wastage.
    union {
        struct {
            int m_size;
            int m_capacity;
        };
        struct {
            void* m_skippedFrame0;
            void* m_skippedFrame1;
        };
    };
    union {
        void** m_borrowedStack;
        void* m_stack[1];
    };
};

} // namespace WTF

using WTF::StackTrace;
