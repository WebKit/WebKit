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

#ifndef FTLExitValue_h
#define FTLExitValue_h

#include <wtf/Platform.h>

#if ENABLE(FTL_JIT)

#include "FTLExitArgument.h"
#include "FTLRecoveryOpcode.h"
#include "JSCJSValue.h"
#include "VirtualRegister.h"
#include <wtf/PrintStream.h>

namespace JSC { namespace FTL {

// This is like ValueRecovery, but respects the way that the FTL does OSR
// exit: the live non-constant non-flushed values are passed as arguments
// to a noreturn tail call. ExitValue is hence mostly responsible for
// telling us the mapping between operands in bytecode and the arguments to
// the call.

enum ExitValueKind {
    InvalidExitValue,
    ExitValueDead,
    ExitValueArgument,
    ExitValueConstant,
    ExitValueInJSStack,
    ExitValueInJSStackAsInt32,
    ExitValueInJSStackAsInt52,
    ExitValueInJSStackAsDouble,
    ExitValueRecovery
};

class ExitValue {
public:
    ExitValue()
        : m_kind(InvalidExitValue)
    {
    }
    
    bool operator!() const { return m_kind == InvalidExitValue; }
    
    static ExitValue dead()
    {
        ExitValue result;
        result.m_kind = ExitValueDead;
        return result;
    }
    
    static ExitValue inJSStack(VirtualRegister reg)
    {
        ExitValue result;
        result.m_kind = ExitValueInJSStack;
        result.u.virtualRegister = reg.offset();
        return result;
    }
    
    static ExitValue inJSStackAsInt32(VirtualRegister reg)
    {
        ExitValue result;
        result.m_kind = ExitValueInJSStackAsInt32;
        result.u.virtualRegister = reg.offset();
        return result;
    }
    
    static ExitValue inJSStackAsInt52(VirtualRegister reg)
    {
        ExitValue result;
        result.m_kind = ExitValueInJSStackAsInt52;
        result.u.virtualRegister = reg.offset();
        return result;
    }
    
    static ExitValue inJSStackAsDouble(VirtualRegister reg)
    {
        ExitValue result;
        result.m_kind = ExitValueInJSStackAsDouble;
        result.u.virtualRegister = reg.offset();
        return result;
    }
    
    static ExitValue constant(JSValue value)
    {
        ExitValue result;
        result.m_kind = ExitValueConstant;
        result.u.constant = JSValue::encode(value);
        return result;
    }
    
    static ExitValue exitArgument(const ExitArgument& argument)
    {
        ExitValue result;
        result.m_kind = ExitValueArgument;
        result.u.argument = argument.representation();
        return result;
    }
    
    static ExitValue recovery(RecoveryOpcode opcode, unsigned leftArgument, unsigned rightArgument, ValueFormat format)
    {
        ExitValue result;
        result.m_kind = ExitValueRecovery;
        result.u.recovery.opcode = opcode;
        result.u.recovery.leftArgument = leftArgument;
        result.u.recovery.rightArgument = rightArgument;
        result.u.recovery.format = format;
        return result;
    }
    
    ExitValueKind kind() const { return m_kind; }
    
    bool isDead() const { return kind() == ExitValueDead; }
    bool isInJSStackSomehow() const
    {
        switch (kind()) {
        case ExitValueInJSStack:
        case ExitValueInJSStackAsInt32:
        case ExitValueInJSStackAsInt52:
        case ExitValueInJSStackAsDouble:
            return true;
        default:
            return false;
        }
    }
    bool isConstant() const { return kind() == ExitValueConstant; }
    bool isArgument() const { return kind() == ExitValueArgument; }
    bool isRecovery() const { return kind() == ExitValueRecovery; }
    
    ExitArgument exitArgument() const
    {
        ASSERT(isArgument());
        return ExitArgument(u.argument);
    }
    
    unsigned leftRecoveryArgument() const
    {
        ASSERT(isRecovery());
        return u.recovery.leftArgument;
    }
    
    unsigned rightRecoveryArgument() const
    {
        ASSERT(isRecovery());
        return u.recovery.rightArgument;
    }
    
    ValueFormat recoveryFormat() const
    {
        ASSERT(isRecovery());
        return static_cast<ValueFormat>(u.recovery.format);
    }
    
    RecoveryOpcode recoveryOpcode() const
    {
        ASSERT(isRecovery());
        return static_cast<RecoveryOpcode>(u.recovery.opcode);
    }
    
    JSValue constant() const
    {
        ASSERT(isConstant());
        return JSValue::decode(u.constant);
    }
    
    VirtualRegister virtualRegister() const
    {
        ASSERT(isInJSStackSomehow());
        return VirtualRegister(u.virtualRegister);
    }
    
    ExitValue withVirtualRegister(VirtualRegister virtualRegister)
    {
        ASSERT(isInJSStackSomehow());
        ExitValue result;
        result.m_kind = m_kind;
        result.u.virtualRegister = virtualRegister.offset();
        return result;
    }

    // If it's in the JSStack somehow, this will tell you what format it's in, in a manner
    // that is compatible with exitArgument().format(). If it's a constant or it's dead, it
    // will claim to be a JSValue. If it's an argument then it will tell you the argument's
    // format.
    ValueFormat valueFormat() const
    {
        switch (kind()) {
        case InvalidExitValue:
            RELEASE_ASSERT_NOT_REACHED();
            return InvalidValueFormat;
            
        case ExitValueDead:
        case ExitValueConstant:
        case ExitValueInJSStack:
            return ValueFormatJSValue;
            
        case ExitValueArgument:
            return exitArgument().format();
            
        case ExitValueInJSStackAsInt32:
            return ValueFormatInt32;
            
        case ExitValueInJSStackAsInt52:
            return ValueFormatInt52;
            
        case ExitValueInJSStackAsDouble:
            return ValueFormatDouble;
            
        case ExitValueRecovery:
            return recoveryFormat();
        }
        
        RELEASE_ASSERT_NOT_REACHED();
        return InvalidValueFormat;
    }

    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;
    
private:
    ExitValueKind m_kind;
    union {
        ExitArgumentRepresentation argument;
        EncodedJSValue constant;
        int virtualRegister;
        struct {
            uint16_t leftArgument;
            uint16_t rightArgument;
            uint16_t opcode;
            uint16_t format;
        } recovery;
    } u;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLExitValue_h
