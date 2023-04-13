/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "DirectEvalExecutable.h"

#include "CodeCache.h"
#include "Debugger.h"
#include "Error.h"
#include "GlobalObjectMethodTable.h"
#include "JSCJSValueInlines.h"
#include "ParserError.h"

namespace JSC {

DirectEvalExecutable* DirectEvalExecutable::create(JSGlobalObject* globalObject, const SourceCode& source, DerivedContextType derivedContextType, NeedsClassFieldInitializer needsClassFieldInitializer, PrivateBrandRequirement privateBrandRequirement, bool isArrowFunctionContext, bool isInsideOrdinaryFunction, EvalContextType evalContextType, const TDZEnvironment* variablesUnderTDZ, const PrivateNameEnvironment* privateNameEnvironment, ECMAMode ecmaMode)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!globalObject->evalEnabled()) {
        globalObject->globalObjectMethodTable()->reportViolationForUnsafeEval(globalObject, source.provider() ? jsNontrivialString(vm, source.provider()->source().toString()) : nullptr);
        throwException(globalObject, scope, createEvalError(globalObject, globalObject->evalDisabledErrorMessage()));
        return nullptr;
    }

    auto* executable = new (NotNull, allocateCell<DirectEvalExecutable>(vm)) DirectEvalExecutable(globalObject, source, ecmaMode.isStrict(), derivedContextType, needsClassFieldInitializer, privateBrandRequirement, isArrowFunctionContext, isInsideOrdinaryFunction, evalContextType);
    executable->finishCreation(vm);

    ParserError error;
    JSParserStrictMode strictMode = ecmaMode.isStrict() ? JSParserStrictMode::Strict : JSParserStrictMode::NotStrict;
    OptionSet<CodeGenerationMode> codeGenerationMode = globalObject->defaultCodeGenerationMode();

    // We don't bother with CodeCache here because direct eval uses a specialized DirectEvalCodeCache.
    UnlinkedEvalCodeBlock* unlinkedEvalCode = generateUnlinkedCodeBlockForDirectEval(vm, executable, executable->source(), strictMode, JSParserScriptMode::Classic, codeGenerationMode, error, evalContextType, variablesUnderTDZ, privateNameEnvironment);

    if (globalObject->hasDebugger())
        globalObject->debugger()->sourceParsed(globalObject, executable->source().provider(), error.line(), error.message());

    if (error.isValid()) {
        throwVMError(globalObject, scope, error.toErrorObject(globalObject, executable->source()));
        return nullptr;
    }

    executable->m_unlinkedCodeBlock.set(vm, executable, unlinkedEvalCode);

    return executable;
}

DirectEvalExecutable::DirectEvalExecutable(JSGlobalObject* globalObject, const SourceCode& source, bool inStrictContext, DerivedContextType derivedContextType, NeedsClassFieldInitializer needsClassFieldInitializer, PrivateBrandRequirement privateBrandRequirement, bool isArrowFunctionContext, bool isInsideOrdinaryFunction, EvalContextType evalContextType)
    : EvalExecutable(globalObject, source, inStrictContext, derivedContextType, isArrowFunctionContext, isInsideOrdinaryFunction, evalContextType, needsClassFieldInitializer, privateBrandRequirement)
{
    ASSERT((needsClassFieldInitializer == NeedsClassFieldInitializer::No && privateBrandRequirement == PrivateBrandRequirement::None) || derivedContextType == DerivedContextType::DerivedConstructorContext);
}

} // namespace JSC
