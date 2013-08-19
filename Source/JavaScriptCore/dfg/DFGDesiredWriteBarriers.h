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

#ifndef DFGDesiredWriteBarriers_h
#define DFGDesiredWriteBarriers_h

#include "WriteBarrier.h"
#include <wtf/Vector.h>

#if ENABLE(DFG_JIT)

namespace JSC {

class VM;

namespace DFG {

class DesiredWriteBarrier {
public:
    DesiredWriteBarrier(WriteBarrier<Unknown>*, JSCell* owner);
    DesiredWriteBarrier(Vector<WriteBarrier<Unknown> >*, unsigned index, JSCell* owner);

    void trigger(VM&);

private:
    JSCell* m_owner;
    enum WriteBarrierType { NormalType, VectorType };
    WriteBarrierType m_type;
    union {
        WriteBarrier<Unknown>* m_barrier;
        struct {
            Vector<WriteBarrier<Unknown> >* m_barriers;
            unsigned m_index;
        } barrier_vector;
    } u;
};

class DesiredWriteBarriers {
public:
    DesiredWriteBarriers();
    ~DesiredWriteBarriers();

    template <typename T>
    DesiredWriteBarrier& add(WriteBarrier<T>& barrier, JSCell* owner)
    {
        return addImpl(reinterpret_cast<WriteBarrier<Unknown>*>(&barrier), owner);
    }

    DesiredWriteBarrier& add(Vector<WriteBarrier<Unknown> >& barriers, unsigned index, JSCell* owner)
    {
        m_barriers.append(DesiredWriteBarrier(&barriers, index, owner));
        return m_barriers.last();
    }

    void trigger(VM&);

private:
    DesiredWriteBarrier& addImpl(WriteBarrier<Unknown>*, JSCell*);

    Vector<DesiredWriteBarrier> m_barriers;
};

template <typename T, typename U>
void initializeLazyWriteBarrier(WriteBarrier<T>& barrier, DesiredWriteBarriers& barriers, JSCell* owner, U value)
{
    barrier = WriteBarrier<T>(barriers.add(barrier, owner), value);
}

void initializeLazyWriteBarrierForConstant(CodeBlock*, DesiredWriteBarriers&, JSCell* owner, JSValue);

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGDesiredWriteBarriers_h
