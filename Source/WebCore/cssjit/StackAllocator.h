/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef StackAllocator_h
#define StackAllocator_h

#if ENABLE(CSS_SELECTOR_JIT)

#include "RegisterAllocator.h"
#include <JavaScriptCore/MacroAssembler.h>

namespace WebCore {

class StackAllocator {
public:
    class StackReference {
    public:
        StackReference()
            : m_offsetFromTop(-1)
        { }
        explicit StackReference(unsigned offset)
            : m_offsetFromTop(offset)
        { }
        operator unsigned() const { return m_offsetFromTop; }
    private:
        unsigned m_offsetFromTop;
    };

    typedef Vector<StackReference, maximumRegisterCount> StackReferenceVector;

    StackAllocator(JSC::MacroAssembler& assembler)
        : m_assembler(assembler)
        , m_offsetFromTop(0)
        , m_hasFunctionCallPadding(false)
    {
    }

    ~StackAllocator()
    {
        RELEASE_ASSERT(!m_offsetFromTop);
        RELEASE_ASSERT(!m_hasFunctionCallPadding);
    }

    StackReference allocateUninitialized()
    {
        RELEASE_ASSERT(!m_hasFunctionCallPadding);
        m_assembler.addPtrNoFlags(JSC::MacroAssembler::TrustedImm32(-stackUnitInBytes()), JSC::MacroAssembler::stackPointerRegister);
        m_offsetFromTop += stackUnitInBytes();
        return StackReference(m_offsetFromTop);
    }

    StackReferenceVector push(const Vector<JSC::MacroAssembler::RegisterID>& registerIDs)
    {
        RELEASE_ASSERT(!m_hasFunctionCallPadding);
        StackReferenceVector stackReferences;
#if CPU(ARM64)
        unsigned pushRegisterCount = registerIDs.size();
        for (unsigned i = 0; i < pushRegisterCount - 1; i += 2) {
            m_assembler.pushPair(registerIDs[i + 1], registerIDs[i]);
            m_offsetFromTop += stackUnitInBytes();
            stackReferences.append(StackReference(m_offsetFromTop - stackUnitInBytes() / 2));
            stackReferences.append(StackReference(m_offsetFromTop));
        }
        if (pushRegisterCount % 2)
            stackReferences.append(push(registerIDs[pushRegisterCount - 1]));
#else
        for (auto registerID : registerIDs)
            stackReferences.append(push(registerID));
#endif
        return stackReferences;
    }

    StackReference push(JSC::MacroAssembler::RegisterID registerID)
    {
        RELEASE_ASSERT(!m_hasFunctionCallPadding);
        m_assembler.pushToSave(registerID);
        m_offsetFromTop += stackUnitInBytes();
        return StackReference(m_offsetFromTop);
    }

    void pop(const StackReferenceVector& stackReferences, const Vector<JSC::MacroAssembler::RegisterID>& registerIDs)
    {
        RELEASE_ASSERT(!m_hasFunctionCallPadding);

        unsigned popRegisterCount = registerIDs.size();
        RELEASE_ASSERT(stackReferences.size() == popRegisterCount);
#if CPU(ARM64)
        ASSERT(m_offsetFromTop >= stackUnitInBytes() * ((popRegisterCount + 1) / 2));
        unsigned popRegisterCountOdd = popRegisterCount % 2;
        if (popRegisterCountOdd)
            pop(stackReferences[popRegisterCount - 1], registerIDs[popRegisterCount - 1]);
        for (unsigned i = popRegisterCount - popRegisterCountOdd; i > 0; i -= 2) {
            RELEASE_ASSERT(stackReferences[i - 1] == m_offsetFromTop);
            RELEASE_ASSERT(stackReferences[i - 2] == m_offsetFromTop - stackUnitInBytes() / 2);
            RELEASE_ASSERT(m_offsetFromTop >= stackUnitInBytes());
            m_offsetFromTop -= stackUnitInBytes();
            m_assembler.popPair(registerIDs[i - 1], registerIDs[i - 2]);
        }
#else
        ASSERT(m_offsetFromTop >= stackUnitInBytes() * popRegisterCount);
        for (unsigned i = popRegisterCount; i > 0; --i)
            pop(stackReferences[i - 1], registerIDs[i - 1]);
#endif
    }

    void pop(StackReference stackReference, JSC::MacroAssembler::RegisterID registerID)
    {
        RELEASE_ASSERT(stackReference == m_offsetFromTop);
        RELEASE_ASSERT(!m_hasFunctionCallPadding);
        RELEASE_ASSERT(m_offsetFromTop >= stackUnitInBytes());
        m_offsetFromTop -= stackUnitInBytes();
        m_assembler.popToRestore(registerID);
    }

    void popAndDiscard(StackReference stackReference)
    {
        RELEASE_ASSERT(stackReference == m_offsetFromTop);
        m_assembler.addPtr(JSC::MacroAssembler::TrustedImm32(stackUnitInBytes()), JSC::MacroAssembler::stackPointerRegister);
        m_offsetFromTop -= stackUnitInBytes();
    }

    void popAndDiscardUpTo(StackReference stackReference)
    {
        unsigned positionBeforeStackReference = stackReference - stackUnitInBytes();
        RELEASE_ASSERT(positionBeforeStackReference < m_offsetFromTop);

        unsigned stackDelta = m_offsetFromTop - positionBeforeStackReference;
        m_assembler.addPtr(JSC::MacroAssembler::TrustedImm32(stackDelta), JSC::MacroAssembler::stackPointerRegister);
        m_offsetFromTop -= stackDelta;
    }

    void alignStackPreFunctionCall()
    {
#if CPU(X86_64)
        RELEASE_ASSERT(!m_hasFunctionCallPadding);
        unsigned topAlignment = stackUnitInBytes();
        if ((topAlignment + m_offsetFromTop) % 16) {
            m_hasFunctionCallPadding = true;
            m_assembler.addPtrNoFlags(JSC::MacroAssembler::TrustedImm32(-stackUnitInBytes()), JSC::MacroAssembler::stackPointerRegister);
        }
#endif
    }

    void unalignStackPostFunctionCall()
    {
#if CPU(X86_64)
        if (m_hasFunctionCallPadding) {
            m_assembler.addPtrNoFlags(JSC::MacroAssembler::TrustedImm32(stackUnitInBytes()), JSC::MacroAssembler::stackPointerRegister);
            m_hasFunctionCallPadding = false;
        }
#endif
    }

    void merge(StackAllocator&& stackA, StackAllocator&& stackB)
    {
        RELEASE_ASSERT(stackA.m_offsetFromTop == stackB.m_offsetFromTop);
        RELEASE_ASSERT(stackA.m_hasFunctionCallPadding == stackB.m_hasFunctionCallPadding);
        ASSERT(&stackA.m_assembler == &stackB.m_assembler);
        ASSERT(&m_assembler == &stackB.m_assembler);

        m_offsetFromTop = stackA.m_offsetFromTop;
        m_hasFunctionCallPadding = stackA.m_hasFunctionCallPadding;

        stackA.reset();
        stackB.reset();
    }

    void merge(StackAllocator&& stackA, StackAllocator&& stackB, StackAllocator&& stackC)
    {
        RELEASE_ASSERT(stackA.m_offsetFromTop == stackB.m_offsetFromTop);
        RELEASE_ASSERT(stackA.m_offsetFromTop == stackC.m_offsetFromTop);
        RELEASE_ASSERT(stackA.m_hasFunctionCallPadding == stackB.m_hasFunctionCallPadding);
        RELEASE_ASSERT(stackA.m_hasFunctionCallPadding == stackC.m_hasFunctionCallPadding);
        ASSERT(&stackA.m_assembler == &stackB.m_assembler);
        ASSERT(&stackA.m_assembler == &stackC.m_assembler);
        ASSERT(&m_assembler == &stackB.m_assembler);

        m_offsetFromTop = stackA.m_offsetFromTop;
        m_hasFunctionCallPadding = stackA.m_hasFunctionCallPadding;

        stackA.reset();
        stackB.reset();
        stackC.reset();
    }

    unsigned offsetToStackReference(StackReference stackReference)
    {
        RELEASE_ASSERT(m_offsetFromTop >= stackReference);
        return m_offsetFromTop - stackReference;
    }

private:
    static unsigned stackUnitInBytes()
    {
        return JSC::MacroAssembler::pushToSaveByteOffset();
    }

    void reset()
    {
        m_offsetFromTop = 0;
        m_hasFunctionCallPadding = false;
    }

    JSC::MacroAssembler& m_assembler;
    unsigned m_offsetFromTop;
    bool m_hasFunctionCallPadding;
};

} // namespace WebCore

#endif // ENABLE(CSS_SELECTOR_JIT)

#endif // StackAllocator_h
