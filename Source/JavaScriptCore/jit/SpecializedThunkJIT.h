/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(JIT)

#include "JIT.h"
#include "JITInlines.h"
#include "JSInterfaceJIT.h"
#include "LinkBuffer.h"
#include "MacroAssembler.h"

namespace JSC {

    class SpecializedThunkJIT : public JSInterfaceJIT {
        WTF_MAKE_TZONE_NON_HEAP_ALLOCATABLE(SpecializedThunkJIT);
    public:
        static constexpr int ThisArgument = -1;
        SpecializedThunkJIT(VM& vm, int expectedArgCount)
            : JSInterfaceJIT(&vm)
        {
            emitFunctionPrologue();
            emitSaveThenMaterializeTagRegisters();
            // Check that we have the expected number of arguments
            m_failures.append(branch32(NotEqual, lowWordFor(CallFrameSlot::argumentCountIncludingThis), TrustedImm32(expectedArgCount + 1)));
        }
        
        explicit SpecializedThunkJIT(VM& vm)
            : JSInterfaceJIT(&vm)
        {
            emitFunctionPrologue();
            emitSaveThenMaterializeTagRegisters();
        }

        void loadJSArgument(int argument, JSValueRegs dst)
        {
            VirtualRegister src = virtualRegisterForArgumentIncludingThis(argument + 1);
            emitLoadJSValue(src, dst);
        }
        
        void loadDoubleArgument(int argument, FPRegisterID dst, RegisterID scratch)
        {
            VirtualRegister src = virtualRegisterForArgumentIncludingThis(argument + 1);
            m_failures.append(emitLoadDouble(src, dst, scratch));
        }
        
        void loadCellArgument(int argument, RegisterID dst)
        {
            VirtualRegister src = virtualRegisterForArgumentIncludingThis(argument + 1);
            m_failures.append(emitLoadJSCell(src, dst));
        }
        
        void loadJSStringArgument(int argument, RegisterID dst)
        {
            loadCellArgument(argument, dst);
            m_failures.append(branchIfNotString(dst));
        }
        
        void loadInt32Argument(int argument, RegisterID dst, Jump& failTarget)
        {
            VirtualRegister src = virtualRegisterForArgumentIncludingThis(argument + 1);
            failTarget = emitLoadInt32(src, dst);
        }
        
        void loadInt32Argument(int argument, RegisterID dst)
        {
            Jump conversionFailed;
            loadInt32Argument(argument, dst, conversionFailed);
            m_failures.append(conversionFailed);
        }
        
        void appendFailure(const Jump& failure)
        {
            m_failures.append(failure);
        }
        void returnJSValue(RegisterID src)
        {
            if (src != regT0)
                move(src, regT0);

            emitRestoreSavedTagRegisters();
            emitFunctionEpilogue();
            ret();
        }
        void returnJSValue(JSValueRegs src)
        {
            if (src != JSRInfo::returnValueJSR)
                moveValueRegs(src, JSRInfo::returnValueJSR);

            emitRestoreSavedTagRegisters();
            emitFunctionEpilogue();
            ret();
        }
        
        void returnDouble(FPRegisterID src)
        {
            moveDoubleTo64(src, regT0);
            Jump zero = branchTest64(Zero, regT0);
            sub64(numberTagRegister, regT0);
            Jump done = jump();
            zero.link(this);
            move(numberTagRegister, regT0);
            done.link(this);
            emitRestoreSavedTagRegisters();
            emitFunctionEpilogue();
            ret();
        }

        void returnInt32(RegisterID src)
        {
            if (src != regT0)
                move(src, regT0);
            tagReturnAsInt32();
            emitRestoreSavedTagRegisters();
            emitFunctionEpilogue();
            ret();
        }

        void returnJSCell(RegisterID src)
        {
            if (src != regT0)
                move(src, regT0);
            tagReturnAsJSCell();
            emitRestoreSavedTagRegisters();
            emitFunctionEpilogue();
            ret();
        }
        
        MacroAssemblerCodeRef<JITThunkPtrTag> finalize(CodePtr<JITThunkPtrTag> fallback, const char* thunkKind)
        {
            m_failures.linkThunk(CodeLocationLabel<JITThunkPtrTag>(fallback), this);
            LinkBuffer patchBuffer(*this, GLOBAL_THUNK_ID, LinkBuffer::Profile::Thunk);
            for (unsigned i = 0; i < m_calls.size(); i++)
                patchBuffer.link(m_calls[i].first, m_calls[i].second);
            return FINALIZE_THUNK(patchBuffer, JITThunkPtrTag, ASCIILiteral::fromLiteralUnsafe(thunkKind), "Specialized thunk for %s", thunkKind);
        }

        // Assumes that the target function uses fpRegister0 as the first argument
        // and return value. Like any sensible architecture would.
        void callDoubleToDouble(CodePtr<CFunctionPtrTag> function)
        {
            m_calls.append(std::make_pair(call(OperationPtrTag), function.retagged<OperationPtrTag>()));
        }
        
        void callDoubleToDoublePreservingReturn(CodePtr<CFunctionPtrTag> function)
        {
            if (!isX86())
                preserveReturnAddressAfterCall(regT3);
            callDoubleToDouble(function);
            if (!isX86())
                restoreReturnAddressBeforeReturn(regT3);
        }

    private:
        void tagReturnAsInt32()
        {
            or64(numberTagRegister, regT0);
        }

        void tagReturnAsJSCell()
        {
        }
        
        MacroAssembler::JumpList m_failures;
        Vector<std::pair<Call, CodePtr<OperationPtrTag>>> m_calls;
    };

}

#endif // ENABLE(JIT)
