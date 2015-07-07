/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef Breakpoint_h
#define Breakpoint_h

#include "DebuggerPrimitives.h"
#include <wtf/DoublyLinkedList.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace JSC {

struct Breakpoint : public DoublyLinkedListNode<Breakpoint> {
    Breakpoint()
        : id(noBreakpointID)
        , sourceID(noSourceID)
        , line(0)
        , column(0)
        , autoContinue(false)
    {
    }

    Breakpoint(SourceID sourceID, unsigned line, unsigned column, String condition, bool autoContinue)
        : id(noBreakpointID)
        , sourceID(sourceID)
        , line(line)
        , column(column)
        , condition(condition)
        , autoContinue(autoContinue)
    {
    }

    Breakpoint(const Breakpoint& other)
        : id(other.id)
        , sourceID(other.sourceID)
        , line(other.line)
        , column(other.column)
        , condition(other.condition)
        , autoContinue(other.autoContinue)
    {
    }

    BreakpointID id;
    SourceID sourceID;
    unsigned line;
    unsigned column;
    String condition;
    bool autoContinue;

    static const unsigned unspecifiedColumn = UINT_MAX;

private:
    Breakpoint* m_prev;
    Breakpoint* m_next;

    friend class WTF::DoublyLinkedListNode<Breakpoint>;
};

class BreakpointsList : public DoublyLinkedList<Breakpoint>,
    public RefCounted<BreakpointsList> {
public:
    ~BreakpointsList()
    {
        Breakpoint* breakpoint;
        while ((breakpoint = removeHead()))
            delete breakpoint;
        ASSERT(isEmpty());
    }
};

} // namespace JSC

#endif // Breakpoint_h
