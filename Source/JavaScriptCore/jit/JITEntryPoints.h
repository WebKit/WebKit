/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(JIT)

#include "GPRInfo.h"
#include "MacroAssemblerCodeRef.h"

namespace JSC {
class VM;
class MacroAssemblerCodeRef;

enum ArgumentsLocation : unsigned {
    StackArgs = 0,
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS >= 4
    RegisterArgs1InRegisters,
    RegisterArgs2InRegisters,
    RegisterArgs3InRegisters,
    RegisterArgs4InRegisters,
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS == 6
    RegisterArgs5InRegisters,
    RegisterArgs6InRegisters,
#endif
    RegisterArgsWithExtraOnStack
#endif
};

// This enum needs to have the same enumerator ordering as ArgumentsLocation.
enum ThunkEntryPointType : unsigned {
    StackArgsEntry = 0,
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS >= 4
    Register1ArgEntry,
    Register2ArgsEntry,
    Register3ArgsEntry,
    Register4ArgsEntry,
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS == 6
    Register5ArgsEntry,
    Register6ArgsEntry,
#endif
#endif
    ThunkEntryPointTypeCount
};

enum EntryPointType {
    StackArgsArityCheckNotRequired,
    StackArgsMustCheckArity,
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS
    RegisterArgsArityCheckNotRequired,
    RegisterArgsPossibleExtraArgs,
    RegisterArgsMustCheckArity,
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS >= 4
    RegisterArgs1,
    RegisterArgs2,
    RegisterArgs3,
    RegisterArgs4,
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS == 6
    RegisterArgs5,
    RegisterArgs6,
#endif
#endif
#endif
    NumberOfEntryPointTypes
};

class JITEntryPoints {
public:
    typedef MacroAssemblerCodePtr CodePtr;
    static const unsigned numberOfEntryTypes = EntryPointType::NumberOfEntryPointTypes;

    JITEntryPoints()
    {
        clearEntries();
    }

    JITEntryPoints(CodePtr registerArgsNoCheckRequiredEntry, CodePtr registerArgsPossibleExtraArgsEntry,
        CodePtr registerArgsCheckArityEntry, CodePtr stackArgsArityCheckNotRequiredEntry,
        CodePtr stackArgsCheckArityEntry)
    {
        m_entryPoints[StackArgsArityCheckNotRequired] = stackArgsArityCheckNotRequiredEntry;
        m_entryPoints[StackArgsMustCheckArity] = stackArgsCheckArityEntry;
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS
        m_entryPoints[RegisterArgsArityCheckNotRequired] = registerArgsNoCheckRequiredEntry;
        m_entryPoints[RegisterArgsPossibleExtraArgs] = registerArgsPossibleExtraArgsEntry;
        m_entryPoints[RegisterArgsMustCheckArity] = registerArgsCheckArityEntry;
        for (unsigned i = 1; i <= NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS; i ++)
            m_entryPoints[registerEntryTypeForArgumentCount(i)] = registerArgsCheckArityEntry;
#else
        UNUSED_PARAM(registerArgsNoCheckRequiredEntry);
        UNUSED_PARAM(registerArgsPossibleExtraArgsEntry);
        UNUSED_PARAM(registerArgsCheckArityEntry);
#endif

    }

    CodePtr entryFor(EntryPointType type)
    {
        return m_entryPoints[type];
    }

    void setEntryFor(EntryPointType type, CodePtr entry)
    {
        ASSERT(type < NumberOfEntryPointTypes);
        m_entryPoints[type] = entry;
    }

    static ptrdiff_t offsetOfEntryFor(EntryPointType type)
    {
        return offsetof(JITEntryPoints, m_entryPoints[type]);
    }

    static EntryPointType registerEntryTypeForArgumentCount(unsigned argCount)
    {
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS
        ASSERT(argCount);
        unsigned registerArgCount = numberOfRegisterArgumentsFor(argCount);
        if (!registerArgCount || registerArgCount != argCount)
            return RegisterArgsMustCheckArity;

        return static_cast<EntryPointType>(RegisterArgs1 + registerArgCount - 1);
#else
        UNUSED_PARAM(argCount);
        RELEASE_ASSERT_NOT_REACHED();
        return StackArgsMustCheckArity;
#endif
    }

    static EntryPointType registerEntryTypeForArgumentType(ArgumentsLocation type)
    {
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS
        ASSERT(type != StackArgs);
        if (type == RegisterArgsWithExtraOnStack)
            return RegisterArgsMustCheckArity;
        
        return static_cast<EntryPointType>(RegisterArgs1 + type - RegisterArgs1InRegisters);
#else
        UNUSED_PARAM(type);
        RELEASE_ASSERT_NOT_REACHED();
        return StackArgsMustCheckArity;
#endif
    }

    void clearEntries()
    {
        for (unsigned i = numberOfEntryTypes; i--;)
            m_entryPoints[i] = MacroAssemblerCodePtr();
    }

    JITEntryPoints& operator=(const JITEntryPoints& other)
    {
        for (unsigned i = numberOfEntryTypes; i--;)
            m_entryPoints[i] = other.m_entryPoints[i];

        return *this;
    }

private:

    CodePtr m_entryPoints[numberOfEntryTypes];
};

class JITEntryPointsWithRef : public JITEntryPoints {
public:
    typedef MacroAssemblerCodeRef CodeRef;

    JITEntryPointsWithRef()
    {
    }

    JITEntryPointsWithRef(const JITEntryPointsWithRef& other)
        : JITEntryPoints(other)
        , m_codeRef(other.m_codeRef)
    {
    }

    JITEntryPointsWithRef(CodeRef codeRef, const JITEntryPoints& other)
        : JITEntryPoints(other)
        , m_codeRef(codeRef)
    {
    }
    
    JITEntryPointsWithRef(CodeRef codeRef, CodePtr stackArgsArityCheckNotRequiredEntry,
        CodePtr stackArgsCheckArityEntry)
        : JITEntryPoints(CodePtr(), CodePtr(), CodePtr(), stackArgsArityCheckNotRequiredEntry, stackArgsCheckArityEntry)
        , m_codeRef(codeRef)
    {
    }

    JITEntryPointsWithRef(CodeRef codeRef, CodePtr registerArgsNoChecksRequiredEntry,
        CodePtr registerArgsPossibleExtraArgsEntry, CodePtr registerArgsCheckArityEntry,
        CodePtr stackArgsArityCheckNotRequiredEntry, CodePtr stackArgsCheckArityEntry)
        : JITEntryPoints(registerArgsNoChecksRequiredEntry, registerArgsPossibleExtraArgsEntry,
            registerArgsCheckArityEntry, stackArgsArityCheckNotRequiredEntry,
            stackArgsCheckArityEntry)
        , m_codeRef(codeRef)
    {
    }

    CodeRef codeRef() { return m_codeRef; }

private:
    CodeRef m_codeRef;
};

inline ArgumentsLocation argumentsLocationFor(unsigned argumentCount)
{
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS
    if (!argumentCount)
        return StackArgs;
    
    argumentCount = std::min(NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS + 1, argumentCount);
    
    return static_cast<ArgumentsLocation>(ArgumentsLocation::RegisterArgs1InRegisters + argumentCount - 1);
#else
    UNUSED_PARAM(argumentCount);
    return StackArgs;
#endif
}

inline EntryPointType registerEntryPointTypeFor(unsigned argumentCount)
{
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS
    if (!argumentCount || argumentCount > NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS)
        return RegisterArgsMustCheckArity;
    
    return static_cast<EntryPointType>(EntryPointType::RegisterArgs1 + argumentCount - 1);
#else
    RELEASE_ASSERT_NOT_REACHED();
    UNUSED_PARAM(argumentCount);
    return StackArgsMustCheckArity;
#endif
}

inline EntryPointType entryPointTypeFor(ArgumentsLocation argumentLocation)
{
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS
    if (argumentLocation == StackArgs)
        return StackArgsMustCheckArity;
    
    if (argumentLocation == RegisterArgsWithExtraOnStack)
        return RegisterArgsMustCheckArity;
    
    return static_cast<EntryPointType>(EntryPointType::RegisterArgs1 + static_cast<unsigned>(argumentLocation - RegisterArgs1InRegisters));
#else
    UNUSED_PARAM(argumentLocation);
    return StackArgsMustCheckArity;
#endif
}

inline ThunkEntryPointType thunkEntryPointTypeFor(ArgumentsLocation argumentLocation)
{
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS
    unsigned argumentLocationIndex = std::min(RegisterArgsWithExtraOnStack - 1, static_cast<unsigned>(argumentLocation));
    return static_cast<ThunkEntryPointType>(argumentLocationIndex);
#else
    UNUSED_PARAM(argumentLocation);
    return StackArgsEntry;
#endif
}

inline ThunkEntryPointType thunkEntryPointTypeFor(unsigned argumentCount)
{
#if NUMBER_OF_JS_FUNCTION_ARGUMENT_REGISTERS
    argumentCount = numberOfRegisterArgumentsFor(argumentCount);
    
    return static_cast<ThunkEntryPointType>(ThunkEntryPointType::Register1ArgEntry + argumentCount - 1);
#else
    UNUSED_PARAM(argumentCount);
    return StackArgsEntry;
#endif
}

class JITJSCallThunkEntryPointsWithRef {
public:
    typedef MacroAssemblerCodePtr CodePtr;
    typedef MacroAssemblerCodeRef CodeRef;
    static const unsigned numberOfEntryTypes = ThunkEntryPointType::ThunkEntryPointTypeCount;

    JITJSCallThunkEntryPointsWithRef()
    {
    }

    JITJSCallThunkEntryPointsWithRef(CodeRef codeRef)
        : m_codeRef(codeRef)
    {
    }

    JITJSCallThunkEntryPointsWithRef(const JITJSCallThunkEntryPointsWithRef& other)
        : m_codeRef(other.m_codeRef)
    {
        for (unsigned i = 0; i < numberOfEntryTypes; i++)
            m_entryPoints[i] = other.m_entryPoints[i];
    }

    CodePtr entryFor(ThunkEntryPointType type)
    {
        return m_entryPoints[type];
    }

    CodePtr entryFor(ArgumentsLocation argumentsLocation)
    {
        return entryFor(thunkEntryPointTypeFor(argumentsLocation));
    }

    void setEntryFor(ThunkEntryPointType type, CodePtr entry)
    {
        m_entryPoints[type] = entry;
    }

    static ptrdiff_t offsetOfEntryFor(ThunkEntryPointType type)
    {
        return offsetof(JITJSCallThunkEntryPointsWithRef, m_entryPoints[type]);
    }

    void clearEntries()
    {
        for (unsigned i = numberOfEntryTypes; i--;)
            m_entryPoints[i] = MacroAssemblerCodePtr();
    }

    CodeRef codeRef() { return m_codeRef; }

    JITJSCallThunkEntryPointsWithRef& operator=(const JITJSCallThunkEntryPointsWithRef& other)
    {
        m_codeRef = other.m_codeRef;
        for (unsigned i = numberOfEntryTypes; i--;)
            m_entryPoints[i] = other.m_entryPoints[i];
        
        return *this;
    }

private:
    CodeRef m_codeRef;
    CodePtr m_entryPoints[numberOfEntryTypes];
};


} // namespace JSC

#endif // ENABLE(JIT)
