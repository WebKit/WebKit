/*
 * Copyright (C) 2012-2014 Apple Inc. All rights reserved.
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

#ifndef VariableWatchpointSet_h
#define VariableWatchpointSet_h

#include "Watchpoint.h"
#include "WriteBarrier.h"

namespace JSC {

class SymbolTable;

class VariableWatchpointSet : public WatchpointSet {
    friend class LLIntOffsetsExtractor;
public:
    VariableWatchpointSet(SymbolTable& symbolTable)
        : WatchpointSet(ClearWatchpoint)
        , m_symbolTable(symbolTable)
    {
    }
    
    ~VariableWatchpointSet() { }
    
    // For the purpose of deciding whether or not to watch this variable, you only need
    // to inspect inferredValue(). If this returns something other than the empty
    // value, then it means that at all future safepoints, this watchpoint set will be
    // in one of these states:
    //
    //    IsWatched: in this case, the variable's value must still be the
    //        inferredValue.
    //
    //    IsInvalidated: in this case the variable's value may be anything but you'll
    //        either notice that it's invalidated and not install the watchpoint, or
    //        you will have been notified that the watchpoint was fired.
    JSValue inferredValue() const { return m_inferredValue.get(); }
    
    inline void notifyWrite(VM&, JSValue);
    
    void invalidate()
    {
        m_inferredValue.clear();
        WatchpointSet::invalidate();
    }
    
    void finalizeUnconditionally()
    {
        ASSERT(!!m_inferredValue == (state() == IsWatched));
        if (!m_inferredValue)
            return;
        JSValue inferredValue = m_inferredValue.get();
        if (!inferredValue.isCell())
            return;
        JSCell* cell = inferredValue.asCell();
        if (Heap::isMarked(cell))
            return;
        invalidate();
    }

    WriteBarrier<Unknown>* addressOfInferredValue() { return &m_inferredValue; }
    
private:
    SymbolTable& m_symbolTable;
    WriteBarrier<Unknown> m_inferredValue;
};

} // namespace JSC

#endif // VariableWatchpointSet_h

