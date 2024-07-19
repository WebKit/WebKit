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
#include "IndirectEvalExecutable.h"

#include "CodeCache.h"
#include "Debugger.h"
#include "Error.h"
#include "GlobalObjectMethodTable.h"
#include "JSCJSValueInlines.h"
#include "ParserError.h"

namespace JSC {

template<typename ErrorHandlerFunctor>
inline IndirectEvalExecutable* IndirectEvalExecutable::createImpl(JSGlobalObject* globalObject, const SourceCode& source, LexicallyScopedFeatures lexicallyScopedFeatures, DerivedContextType derivedContextType, bool isArrowFunctionContext, EvalContextType evalContextType, ErrorHandlerFunctor errorHandler)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!globalObject->evalEnabled()) {
        globalObject->globalObjectMethodTable()->reportViolationForUnsafeEval(globalObject, source.provider() ? jsNontrivialString(vm, source.provider()->source().toString()) : nullptr);
        throwException(globalObject, scope, createEvalError(globalObject, globalObject->evalDisabledErrorMessage()));
        return nullptr;
    }

    auto* executable = new (NotNull, allocateCell<IndirectEvalExecutable>(vm)) IndirectEvalExecutable(globalObject, source, lexicallyScopedFeatures, derivedContextType, isArrowFunctionContext, evalContextType);
    executable->finishCreation(vm);

    ParserError error;
    OptionSet<CodeGenerationMode> codeGenerationMode = globalObject->defaultCodeGenerationMode();

    UnlinkedEvalCodeBlock* unlinkedEvalCode = vm.codeCache()->getUnlinkedEvalCodeBlock(
        vm, executable, executable->source(), codeGenerationMode, error, evalContextType);

    if (globalObject->hasDebugger())
        globalObject->debugger()->sourceParsed(globalObject, executable->source().provider(), error.line(), error.message());

    if (error.isValid()) {
        errorHandler(executable->source(), &error);
        scope.release();
        return nullptr;
    }

    executable->m_unlinkedCodeBlock.set(vm, executable, unlinkedEvalCode);

    return executable;
}

IndirectEvalExecutable* IndirectEvalExecutable::create(JSGlobalObject* globalObject, const SourceCode& source, LexicallyScopedFeatures lexicallyScopedFeatures, DerivedContextType derivedContextType, bool isArrowFunctionContext, EvalContextType evalContextType, NakedPtr<JSObject>& resultingError)
{
    auto handleError = [&](const SourceCode& source, ParserError* error) {
        resultingError = error->toErrorObject(globalObject, source);
    };
    return createImpl(globalObject, source, lexicallyScopedFeatures, derivedContextType, isArrowFunctionContext, evalContextType, handleError);
}

IndirectEvalExecutable* IndirectEvalExecutable::tryCreate(JSGlobalObject* globalObject, const SourceCode& source, LexicallyScopedFeatures lexicallyScopedFeatures, DerivedContextType derivedContextType, bool isArrowFunctionContext, EvalContextType evalContextType)
{
    VM& vm = globalObject->vm();
    auto handleError = [&](const SourceCode& source, ParserError* error) {
        auto scope = DECLARE_THROW_SCOPE(vm);
        throwVMError(globalObject, scope, error->toErrorObject(globalObject, source));
    };
    return createImpl(globalObject, source, lexicallyScopedFeatures, derivedContextType, isArrowFunctionContext, evalContextType, handleError);
}

constexpr bool insideOrdinaryFunction = false;
IndirectEvalExecutable::IndirectEvalExecutable(JSGlobalObject* globalObject, const SourceCode& source, LexicallyScopedFeatures lexicallyScopedFeatures, DerivedContextType derivedContextType, bool isArrowFunctionContext, EvalContextType evalContextType)
    : EvalExecutable(globalObject, source, lexicallyScopedFeatures, derivedContextType, isArrowFunctionContext, insideOrdinaryFunction, evalContextType, NeedsClassFieldInitializer::No, PrivateBrandRequirement::None)
{
}

} // namespace JSC
