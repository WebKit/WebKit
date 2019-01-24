/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSType.h"

#include <wtf/PrintStream.h>

namespace WTF {

class PrintStream;

#define CASE(__type) \
    case JSC::__type: \
        out.print(#__type); \
        return;

void printInternal(PrintStream& out, JSC::JSType type)
{
    switch (type) {
    CASE(CellType)
    CASE(StringType)
    CASE(SymbolType)
    CASE(BigIntType)
    CASE(CustomGetterSetterType)
    CASE(APIValueWrapperType)
    CASE(ProgramExecutableType)
    CASE(ModuleProgramExecutableType)
    CASE(EvalExecutableType)
    CASE(FunctionExecutableType)
    CASE(UnlinkedFunctionExecutableType)
    CASE(UnlinkedProgramCodeBlockType)
    CASE(UnlinkedModuleProgramCodeBlockType)
    CASE(UnlinkedEvalCodeBlockType)
    CASE(UnlinkedFunctionCodeBlockType)
    CASE(CodeBlockType)
    CASE(JSFixedArrayType)
    CASE(JSImmutableButterflyType)
    CASE(JSSourceCodeType)
    CASE(JSScriptFetcherType)
    CASE(JSScriptFetchParametersType)
    CASE(ObjectType)
    CASE(FinalObjectType)
    CASE(JSCalleeType)
    CASE(JSFunctionType)
    CASE(InternalFunctionType)
    CASE(NumberObjectType)
    CASE(ErrorInstanceType)
    CASE(PureForwardingProxyType)
    CASE(ImpureProxyType)
    CASE(DirectArgumentsType)
    CASE(ScopedArgumentsType)
    CASE(ClonedArgumentsType)
    CASE(ArrayType)
    CASE(DerivedArrayType)
    CASE(ArrayBufferType)
    CASE(Int8ArrayType)
    CASE(Uint8ArrayType)
    CASE(Uint8ClampedArrayType)
    CASE(Int16ArrayType)
    CASE(Uint16ArrayType)
    CASE(Int32ArrayType)
    CASE(Uint32ArrayType)
    CASE(Float32ArrayType)
    CASE(Float64ArrayType)
    CASE(DataViewType)
    CASE(GetterSetterType)
    CASE(GlobalObjectType)
    CASE(GlobalLexicalEnvironmentType)
    CASE(LexicalEnvironmentType)
    CASE(ModuleEnvironmentType)
    CASE(StrictEvalActivationType)
    CASE(WithScopeType)
    CASE(RegExpObjectType)
    CASE(ProxyObjectType)
    CASE(JSMapType)
    CASE(JSSetType)
    CASE(JSWeakMapType)
    CASE(JSWeakSetType)
    CASE(WebAssemblyToJSCalleeType)
    CASE(StringObjectType)
    CASE(MaxJSType)
    }
}

#undef CASE

} // namespace WTF
