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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GCLogging.h"

#include "ClassInfo.h"
#include "Heap.h"
#include "HeapIterationScope.h"
#include "JSCell.h"
#include "JSCellInlines.h"

namespace JSC {

const char* GCLogging::levelAsString(Level level)
{
    switch (level) {
    case None:
        return "None";
    case Basic:
        return "Basic";
    case Verbose:
        return "Verbose";
    default:
        RELEASE_ASSERT_NOT_REACHED();
        return "";
    }
}

class LoggingFunctor {
public:
    LoggingFunctor(SlotVisitor& slotVisitor)
        : m_slotVisitor(slotVisitor)
    {
        m_savedMarkStack.resize(m_slotVisitor.markStack().size());
        m_slotVisitor.markStack().fillVector(m_savedMarkStack);
    }

    ~LoggingFunctor()
    {
        reviveCells();
    }

    void operator()(JSCell* cell)
    {
        m_liveCells.append(cell);
        MarkedBlock::blockFor(cell)->clearMarked(cell);
    }

    void log()
    {
        m_slotVisitor.clearMarkStack();
        for (JSCell* cell : m_liveCells) {
            cell->methodTable()->visitChildren(cell, m_slotVisitor);
            dataLog("\n", *cell, ":\n", m_slotVisitor);
            for (const JSCell* neighbor : m_slotVisitor.markStack())
                MarkedBlock::blockFor(neighbor)->clearMarked(neighbor);
            m_slotVisitor.clearMarkStack();
        }
        m_slotVisitor.reset();
    }

    void reviveCells()
    {
        for (JSCell* cell : m_liveCells)
            MarkedBlock::blockFor(cell)->setMarked(cell);

        for (const JSCell* cell : m_savedMarkStack) {
            m_slotVisitor.markStack().append(cell);
            const_cast<JSCell*>(cell)->setRemembered(true);
        }
    }

    typedef void ReturnType;

    void returnValue() { };

private:
    Vector<const JSCell*> m_savedMarkStack;
    Vector<JSCell*> m_liveCells;
    SlotVisitor& m_slotVisitor;
};

void GCLogging::dumpObjectGraph(Heap* heap)
{
    LoggingFunctor loggingFunctor(heap->m_slotVisitor);
    HeapIterationScope iterationScope(*heap);
    heap->objectSpace().forEachLiveCell(iterationScope, loggingFunctor);
    loggingFunctor.log();
}

} // namespace JSC
