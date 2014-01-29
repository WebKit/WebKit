/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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

#ifndef ValueRecovery_h
#define ValueRecovery_h

#include "DataFormat.h"
#if ENABLE(JIT)
#include "GPRInfo.h"
#include "FPRInfo.h"
#endif
#include "JSCJSValue.h"
#include "MacroAssembler.h"
#include "VirtualRegister.h"
#include <wtf/Platform.h>

namespace JSC {

struct DumpContext;

// Describes how to recover a given bytecode virtual register at a given
// code point.
enum ValueRecoveryTechnique {
    // It's in a register.
    InGPR,
    UnboxedInt32InGPR,
    UnboxedInt52InGPR,
    UnboxedStrictInt52InGPR,
    UnboxedBooleanInGPR,
    UnboxedCellInGPR,
#if USE(JSVALUE32_64)
    InPair,
#endif
    InFPR,
    // It's in the stack, but at a different location.
    DisplacedInJSStack,
    // It's in the stack, at a different location, and it's unboxed.
    Int32DisplacedInJSStack,
    Int52DisplacedInJSStack,
    StrictInt52DisplacedInJSStack,
    DoubleDisplacedInJSStack,
    CellDisplacedInJSStack,
    BooleanDisplacedInJSStack,
    // It's an Arguments object.
    ArgumentsThatWereNotCreated,
    // It's a constant.
    Constant,
    // Don't know how to recover it.
    DontKnow
};

class ValueRecovery {
public:
    ValueRecovery()
        : m_technique(DontKnow)
    {
    }
    
    bool isSet() const { return m_technique != DontKnow; }
    bool operator!() const { return !isSet(); }
    
    static ValueRecovery inGPR(MacroAssembler::RegisterID gpr, DataFormat dataFormat)
    {
        ASSERT(dataFormat != DataFormatNone);
#if USE(JSVALUE32_64)
        ASSERT(dataFormat == DataFormatInt32 || dataFormat == DataFormatCell || dataFormat == DataFormatBoolean);
#endif
        ValueRecovery result;
        if (dataFormat == DataFormatInt32)
            result.m_technique = UnboxedInt32InGPR;
        else if (dataFormat == DataFormatInt52)
            result.m_technique = UnboxedInt52InGPR;
        else if (dataFormat == DataFormatStrictInt52)
            result.m_technique = UnboxedStrictInt52InGPR;
        else if (dataFormat == DataFormatBoolean)
            result.m_technique = UnboxedBooleanInGPR;
        else if (dataFormat == DataFormatCell)
            result.m_technique = UnboxedCellInGPR;
        else
            result.m_technique = InGPR;
        result.m_source.gpr = gpr;
        return result;
    }
    
#if USE(JSVALUE32_64)
    static ValueRecovery inPair(MacroAssembler::RegisterID tagGPR, MacroAssembler::RegisterID payloadGPR)
    {
        ValueRecovery result;
        result.m_technique = InPair;
        result.m_source.pair.tagGPR = tagGPR;
        result.m_source.pair.payloadGPR = payloadGPR;
        return result;
    }
#endif

    static ValueRecovery inFPR(MacroAssembler::FPRegisterID fpr)
    {
        ValueRecovery result;
        result.m_technique = InFPR;
        result.m_source.fpr = fpr;
        return result;
    }
    
    static ValueRecovery displacedInJSStack(VirtualRegister virtualReg, DataFormat dataFormat)
    {
        ValueRecovery result;
        switch (dataFormat) {
        case DataFormatInt32:
            result.m_technique = Int32DisplacedInJSStack;
            break;
            
        case DataFormatInt52:
            result.m_technique = Int52DisplacedInJSStack;
            break;
            
        case DataFormatStrictInt52:
            result.m_technique = StrictInt52DisplacedInJSStack;
            break;
            
        case DataFormatDouble:
            result.m_technique = DoubleDisplacedInJSStack;
            break;

        case DataFormatCell:
            result.m_technique = CellDisplacedInJSStack;
            break;
            
        case DataFormatBoolean:
            result.m_technique = BooleanDisplacedInJSStack;
            break;
            
        default:
            ASSERT(dataFormat != DataFormatNone && dataFormat != DataFormatStorage);
            result.m_technique = DisplacedInJSStack;
            break;
        }
        result.m_source.virtualReg = virtualReg.offset();
        return result;
    }
    
    static ValueRecovery constant(JSValue value)
    {
        ValueRecovery result;
        result.m_technique = Constant;
        result.m_source.constant = JSValue::encode(value);
        return result;
    }
    
    static ValueRecovery argumentsThatWereNotCreated()
    {
        ValueRecovery result;
        result.m_technique = ArgumentsThatWereNotCreated;
        return result;
    }
    
    ValueRecoveryTechnique technique() const { return m_technique; }
    
    bool isConstant() const { return m_technique == Constant; }
    
    bool isInRegisters() const
    {
        switch (m_technique) {
        case InGPR:
        case UnboxedInt32InGPR:
        case UnboxedBooleanInGPR:
        case UnboxedCellInGPR:
        case UnboxedInt52InGPR:
        case UnboxedStrictInt52InGPR:
#if USE(JSVALUE32_64)
        case InPair:
#endif
        case InFPR:
            return true;
        default:
            return false;
        }
    }
    
    MacroAssembler::RegisterID gpr() const
    {
        ASSERT(m_technique == InGPR || m_technique == UnboxedInt32InGPR || m_technique == UnboxedBooleanInGPR || m_technique == UnboxedInt52InGPR || m_technique == UnboxedStrictInt52InGPR || m_technique == UnboxedCellInGPR);
        return m_source.gpr;
    }
    
#if USE(JSVALUE32_64)
    MacroAssembler::RegisterID tagGPR() const
    {
        ASSERT(m_technique == InPair);
        return m_source.pair.tagGPR;
    }
    
    MacroAssembler::RegisterID payloadGPR() const
    {
        ASSERT(m_technique == InPair);
        return m_source.pair.payloadGPR;
    }
#endif
    
    MacroAssembler::FPRegisterID fpr() const
    {
        ASSERT(m_technique == InFPR);
        return m_source.fpr;
    }
    
    VirtualRegister virtualRegister() const
    {
        ASSERT(m_technique == DisplacedInJSStack || m_technique == Int32DisplacedInJSStack || m_technique == DoubleDisplacedInJSStack || m_technique == CellDisplacedInJSStack || m_technique == BooleanDisplacedInJSStack || m_technique == Int52DisplacedInJSStack || m_technique == StrictInt52DisplacedInJSStack);
        return VirtualRegister(m_source.virtualReg);
    }
    
    ValueRecovery withLocalsOffset(int offset) const
    {
        switch (m_technique) {
        case DisplacedInJSStack:
        case Int32DisplacedInJSStack:
        case DoubleDisplacedInJSStack:
        case CellDisplacedInJSStack:
        case BooleanDisplacedInJSStack:
        case Int52DisplacedInJSStack:
        case StrictInt52DisplacedInJSStack: {
            ValueRecovery result;
            result.m_technique = m_technique;
            result.m_source.virtualReg = m_source.virtualReg + offset;
            return result;
        }
            
        default:
            return *this;
        }
    }
    
    JSValue constant() const
    {
        ASSERT(m_technique == Constant);
        return JSValue::decode(m_source.constant);
    }
    
    JSValue recover(ExecState*) const;
    
#if ENABLE(JIT)
    void dumpInContext(PrintStream& out, DumpContext* context) const;
    void dump(PrintStream& out) const;
#endif

private:
    ValueRecoveryTechnique m_technique;
    union {
        MacroAssembler::RegisterID gpr;
        MacroAssembler::FPRegisterID fpr;
#if USE(JSVALUE32_64)
        struct {
            MacroAssembler::RegisterID tagGPR;
            MacroAssembler::RegisterID payloadGPR;
        } pair;
#endif
        int virtualReg;
        EncodedJSValue constant;
    } m_source;
};

} // namespace JSC

#endif // ValueRecovery_h
