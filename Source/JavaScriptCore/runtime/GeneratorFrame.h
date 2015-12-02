/*
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>.
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

#ifndef GeneratorFrame_h
#define GeneratorFrame_h

#include "JSCell.h"
#include <wtf/FastBitVector.h>

namespace JSC {

class GeneratorFrame : public JSCell {
    friend class JIT;
#if ENABLE(DFG_JIT)
    friend class DFG::SpeculativeJIT;
    friend class DFG::JITCompiler;
#endif
    friend class VM;
public:
    typedef JSCell Base;
    static const unsigned StructureFlags = StructureIsImmortal | Base::StructureFlags;

    DECLARE_EXPORT_INFO;

    static GeneratorFrame* create(VM&, size_t numberOfCalleeLocals);

    WriteBarrierBase<Unknown>* locals()
    {
        return bitwise_cast<WriteBarrierBase<Unknown>*>(bitwise_cast<char*>(this) + offsetOfLocals());
    }

    WriteBarrierBase<Unknown>& localAt(size_t index)
    {
        ASSERT(index < m_numberOfCalleeLocals);
        return locals()[index];
    }

    static size_t offsetOfLocals()
    {
        return WTF::roundUpToMultipleOf<sizeof(WriteBarrier<Unknown>)>(sizeof(GeneratorFrame));
    }

    static size_t allocationSizeForLocals(unsigned numberOfLocals)
    {
        return offsetOfLocals() + numberOfLocals * sizeof(WriteBarrier<Unknown>);
    }

    static Structure* createStructure(VM&, JSGlobalObject*, JSValue prototype);

    void save(ExecState*, const FastBitVector& liveCalleeLocals);
    void resume(ExecState*, const FastBitVector& liveCalleeLocals);

private:
    GeneratorFrame(VM&, size_t numberOfCalleeLocals);

    size_t m_numberOfCalleeLocals;

    friend class LLIntOffsetsExtractor;

    void finishCreation(VM&);

protected:
    static void visitChildren(JSCell*, SlotVisitor&);
};

} // namespace JSC

#endif // GeneratorFrame_h
