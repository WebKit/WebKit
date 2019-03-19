/*
 * Copyright (C) 2010, 2016 Apple Inc. All rights reserved.
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

#include "BytecodeConventions.h"
#include "CCallHelpers.h"
#include "FPRInfo.h"
#include "GPRInfo.h"
#include "JSCJSValue.h"
#include "JSString.h"
#include "MacroAssembler.h"

#if ENABLE(JIT)

namespace JSC {
    class JSInterfaceJIT : public CCallHelpers, public GPRInfo, public FPRInfo {
    public:

        JSInterfaceJIT(VM* vm = nullptr, CodeBlock* codeBlock = nullptr)
            : CCallHelpers(codeBlock)
            , m_vm(vm)
        {
        }

        inline Jump emitLoadJSCell(unsigned virtualRegisterIndex, RegisterID payload);
        inline Jump emitLoadInt32(unsigned virtualRegisterIndex, RegisterID dst);
        inline Jump emitLoadDouble(unsigned virtualRegisterIndex, FPRegisterID dst, RegisterID scratch);

#if USE(JSVALUE32_64)
        inline Jump emitJumpIfNotJSCell(unsigned virtualRegisterIndex);
#endif

        void emitGetFromCallFrameHeaderPtr(int entry, RegisterID to, RegisterID from = callFrameRegister);
        void emitPutToCallFrameHeader(RegisterID from, int entry);
        void emitPutToCallFrameHeader(void* value, int entry);
        void emitPutCellToCallFrameHeader(RegisterID from, int entry);

        VM* vm() const { return m_vm; }

        VM* m_vm;
    };

#if USE(JSVALUE32_64)
    inline JSInterfaceJIT::Jump JSInterfaceJIT::emitLoadJSCell(unsigned virtualRegisterIndex, RegisterID payload)
    {
        loadPtr(payloadFor(virtualRegisterIndex), payload);
        return emitJumpIfNotJSCell(virtualRegisterIndex);
    }

    inline JSInterfaceJIT::Jump JSInterfaceJIT::emitJumpIfNotJSCell(unsigned virtualRegisterIndex)
    {
        ASSERT(static_cast<int>(virtualRegisterIndex) < FirstConstantRegisterIndex);
        return branch32(NotEqual, tagFor(virtualRegisterIndex), TrustedImm32(JSValue::CellTag));
    }
    
    inline JSInterfaceJIT::Jump JSInterfaceJIT::emitLoadInt32(unsigned virtualRegisterIndex, RegisterID dst)
    {
        ASSERT(static_cast<int>(virtualRegisterIndex) < FirstConstantRegisterIndex);
        loadPtr(payloadFor(virtualRegisterIndex), dst);
        return branch32(NotEqual, tagFor(static_cast<int>(virtualRegisterIndex)), TrustedImm32(JSValue::Int32Tag));
    }

    inline JSInterfaceJIT::Jump JSInterfaceJIT::emitLoadDouble(unsigned virtualRegisterIndex, FPRegisterID dst, RegisterID scratch)
    {
        ASSERT(static_cast<int>(virtualRegisterIndex) < FirstConstantRegisterIndex);
        loadPtr(tagFor(virtualRegisterIndex), scratch);
        Jump isDouble = branch32(Below, scratch, TrustedImm32(JSValue::LowestTag));
        Jump notInt = branch32(NotEqual, scratch, TrustedImm32(JSValue::Int32Tag));
        loadPtr(payloadFor(virtualRegisterIndex), scratch);
        convertInt32ToDouble(scratch, dst);
        Jump done = jump();
        isDouble.link(this);
        loadDouble(addressFor(virtualRegisterIndex), dst);
        done.link(this);
        return notInt;
    }

#endif

#if USE(JSVALUE64)
    inline JSInterfaceJIT::Jump JSInterfaceJIT::emitLoadJSCell(unsigned virtualRegisterIndex, RegisterID dst)
    {
        load64(addressFor(virtualRegisterIndex), dst);
        return branchIfNotCell(dst);
    }
    
    inline JSInterfaceJIT::Jump JSInterfaceJIT::emitLoadInt32(unsigned virtualRegisterIndex, RegisterID dst)
    {
        load64(addressFor(virtualRegisterIndex), dst);
        Jump notInt32 = branchIfNotInt32(dst);
        zeroExtend32ToPtr(dst, dst);
        return notInt32;
    }

    inline JSInterfaceJIT::Jump JSInterfaceJIT::emitLoadDouble(unsigned virtualRegisterIndex, FPRegisterID dst, RegisterID scratch)
    {
        load64(addressFor(virtualRegisterIndex), scratch);
        Jump notNumber = branchIfNotNumber(scratch);
        Jump notInt = branchIfNotInt32(scratch);
        convertInt32ToDouble(scratch, dst);
        Jump done = jump();
        notInt.link(this);
        unboxDouble(scratch, scratch, dst);
        done.link(this);
        return notNumber;
    }
#endif

    ALWAYS_INLINE void JSInterfaceJIT::emitGetFromCallFrameHeaderPtr(int entry, RegisterID to, RegisterID from)
    {
        loadPtr(Address(from, entry * sizeof(Register)), to);
    }

    ALWAYS_INLINE void JSInterfaceJIT::emitPutToCallFrameHeader(RegisterID from, int entry)
    {
#if USE(JSVALUE32_64)
        storePtr(from, payloadFor(entry));
#else
        store64(from, addressFor(entry));
#endif
    }

    ALWAYS_INLINE void JSInterfaceJIT::emitPutToCallFrameHeader(void* value, int entry)
    {
        storePtr(TrustedImmPtr(value), addressFor(entry));
    }

    ALWAYS_INLINE void JSInterfaceJIT::emitPutCellToCallFrameHeader(RegisterID from, int entry)
    {
#if USE(JSVALUE32_64)
        store32(TrustedImm32(JSValue::CellTag), tagFor(entry));
        store32(from, payloadFor(entry));
#else
        store64(from, addressFor(entry));
#endif
    }

} // namespace JSC

#endif // ENABLE(JIT)
