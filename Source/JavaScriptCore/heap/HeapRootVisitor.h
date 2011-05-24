/*
 * Copyright (C) 2009, 2011 Apple Inc. All rights reserved.
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

#ifndef HeapRootVisitor_h
#define HeapRootVisitor_h

#include "MarkStack.h"

namespace JSC {

    // Privileged class for marking JSValues directly. It is only safe to use
    // this class to mark direct heap roots that are marked during every GC pass.
    // All other references should be wrapped in WriteBarriers and marked through
    // the MarkStack.
    class HeapRootVisitor {
    private:
        friend class Heap;
        HeapRootVisitor(SlotVisitor&);

    public:
        void mark(JSValue*);
        void mark(JSValue*, size_t);
        void mark(JSString**);
        void mark(JSCell**);
        
        SlotVisitor& visitor();

    private:
        SlotVisitor& m_visitor;
    };

    inline HeapRootVisitor::HeapRootVisitor(SlotVisitor& visitor)
        : m_visitor(visitor)
    {
    }

    inline void HeapRootVisitor::mark(JSValue* slot)
    {
        m_visitor.append(slot);
    }

    inline void HeapRootVisitor::mark(JSValue* slot, size_t count)
    {
        m_visitor.append(slot, count);
    }

    inline void HeapRootVisitor::mark(JSString** slot)
    {
        m_visitor.append(reinterpret_cast<JSCell**>(slot));
    }

    inline void HeapRootVisitor::mark(JSCell** slot)
    {
        m_visitor.append(slot);
    }

    inline SlotVisitor& HeapRootVisitor::visitor()
    {
        return m_visitor;
    }

} // namespace JSC

#endif // HeapRootVisitor_h
