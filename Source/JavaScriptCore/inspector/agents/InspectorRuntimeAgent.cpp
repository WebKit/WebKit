/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorRuntimeAgent.h"

#if ENABLE(INSPECTOR)

#include "Completion.h"
#include "HeapIterationScope.h"
#include "InjectedScript.h"
#include "InjectedScriptManager.h"
#include "InspectorValues.h"
#include "JSLock.h"
#include "ParserError.h"
#include "ScriptDebugServer.h"
#include "SourceCode.h"
#include "TypeProfiler.h"
#include "TypeProfilerLog.h"
#include "VMEntryScope.h"
#include <wtf/PassRefPtr.h>
#include <wtf/CurrentTime.h>

using namespace JSC;

namespace Inspector {

static bool asBool(const bool* const b)
{
    return b ? *b : false;
}

InspectorRuntimeAgent::InspectorRuntimeAgent(InjectedScriptManager* injectedScriptManager)
    : InspectorAgentBase(ASCIILiteral("Runtime"))
    , m_injectedScriptManager(injectedScriptManager)
    , m_scriptDebugServer(nullptr)
    , m_enabled(false)
    , m_isTypeProfilingEnabled(false)
{
}

InspectorRuntimeAgent::~InspectorRuntimeAgent()
{
}

static ScriptDebugServer::PauseOnExceptionsState setPauseOnExceptionsState(ScriptDebugServer* scriptDebugServer, ScriptDebugServer::PauseOnExceptionsState newState)
{
    ASSERT(scriptDebugServer);
    ScriptDebugServer::PauseOnExceptionsState presentState = scriptDebugServer->pauseOnExceptionsState();
    if (presentState != newState)
        scriptDebugServer->setPauseOnExceptionsState(newState);
    return presentState;
}

static PassRefPtr<Inspector::Protocol::Runtime::ErrorRange> buildErrorRangeObject(const JSTokenLocation& tokenLocation)
{
    RefPtr<Inspector::Protocol::Runtime::ErrorRange> result = Inspector::Protocol::Runtime::ErrorRange::create()
        .setStartOffset(tokenLocation.startOffset)
        .setEndOffset(tokenLocation.endOffset);
    return result.release();
}

void InspectorRuntimeAgent::parse(ErrorString*, const String& expression, Inspector::Protocol::Runtime::SyntaxErrorType* result, Inspector::Protocol::OptOutput<String>* message, RefPtr<Inspector::Protocol::Runtime::ErrorRange>& range)
{
    VM& vm = globalVM();
    JSLockHolder lock(vm);

    ParserError error;
    checkSyntax(vm, JSC::makeSource(expression), error);

    switch (error.m_syntaxErrorType) {
    case ParserError::SyntaxErrorNone:
        *result = Inspector::Protocol::Runtime::SyntaxErrorType::None;
        break;
    case ParserError::SyntaxErrorIrrecoverable:
        *result = Inspector::Protocol::Runtime::SyntaxErrorType::Irrecoverable;
        break;
    case ParserError::SyntaxErrorUnterminatedLiteral:
        *result = Inspector::Protocol::Runtime::SyntaxErrorType::UnterminatedLiteral;
        break;
    case ParserError::SyntaxErrorRecoverable:
        *result = Inspector::Protocol::Runtime::SyntaxErrorType::Recoverable;
        break;
    }

    if (error.m_syntaxErrorType != ParserError::SyntaxErrorNone) {
        *message = error.m_message;
        range = buildErrorRangeObject(error.m_token.m_location);
    }
}

void InspectorRuntimeAgent::evaluate(ErrorString* errorString, const String& expression, const String* const objectGroup, const bool* const includeCommandLineAPI, const bool* const doNotPauseOnExceptionsAndMuteConsole, const int* executionContextId, const bool* const returnByValue, const bool* generatePreview, RefPtr<Inspector::Protocol::Runtime::RemoteObject>& result, Inspector::Protocol::OptOutput<bool>* wasThrown)
{
    InjectedScript injectedScript = injectedScriptForEval(errorString, executionContextId);
    if (injectedScript.hasNoValue())
        return;

    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = ScriptDebugServer::DontPauseOnExceptions;
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        previousPauseOnExceptionsState = setPauseOnExceptionsState(m_scriptDebugServer, ScriptDebugServer::DontPauseOnExceptions);
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        muteConsole();

    injectedScript.evaluate(errorString, expression, objectGroup ? *objectGroup : "", asBool(includeCommandLineAPI), asBool(returnByValue), asBool(generatePreview), &result, wasThrown);

    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
        unmuteConsole();
        setPauseOnExceptionsState(m_scriptDebugServer, previousPauseOnExceptionsState);
    }
}

void InspectorRuntimeAgent::callFunctionOn(ErrorString* errorString, const String& objectId, const String& expression, const RefPtr<InspectorArray>* const optionalArguments, const bool* const doNotPauseOnExceptionsAndMuteConsole, const bool* const returnByValue, const bool* generatePreview, RefPtr<Inspector::Protocol::Runtime::RemoteObject>& result, Inspector::Protocol::OptOutput<bool>* wasThrown)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue()) {
        *errorString = ASCIILiteral("Inspected frame has gone");
        return;
    }

    String arguments;
    if (optionalArguments)
        arguments = (*optionalArguments)->toJSONString();

    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = ScriptDebugServer::DontPauseOnExceptions;
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        previousPauseOnExceptionsState = setPauseOnExceptionsState(m_scriptDebugServer, ScriptDebugServer::DontPauseOnExceptions);
    if (asBool(doNotPauseOnExceptionsAndMuteConsole))
        muteConsole();

    injectedScript.callFunctionOn(errorString, objectId, expression, arguments, asBool(returnByValue), asBool(generatePreview), &result, wasThrown);

    if (asBool(doNotPauseOnExceptionsAndMuteConsole)) {
        unmuteConsole();
        setPauseOnExceptionsState(m_scriptDebugServer, previousPauseOnExceptionsState);
    }
}

void InspectorRuntimeAgent::getProperties(ErrorString* errorString, const String& objectId, const bool* const ownProperties, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Runtime::PropertyDescriptor>>& result, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Runtime::InternalPropertyDescriptor>>& internalProperties)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (injectedScript.hasNoValue()) {
        *errorString = ASCIILiteral("Inspected frame has gone");
        return;
    }

    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = setPauseOnExceptionsState(m_scriptDebugServer, ScriptDebugServer::DontPauseOnExceptions);
    muteConsole();

    injectedScript.getProperties(errorString, objectId, ownProperties ? *ownProperties : false, &result);
    injectedScript.getInternalProperties(errorString, objectId, &internalProperties);

    unmuteConsole();
    setPauseOnExceptionsState(m_scriptDebugServer, previousPauseOnExceptionsState);
}

void InspectorRuntimeAgent::releaseObject(ErrorString*, const String& objectId)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(objectId);
    if (!injectedScript.hasNoValue())
        injectedScript.releaseObject(objectId);
}

void InspectorRuntimeAgent::releaseObjectGroup(ErrorString*, const String& objectGroup)
{
    m_injectedScriptManager->releaseObjectGroup(objectGroup);
}

void InspectorRuntimeAgent::run(ErrorString*)
{
}

void InspectorRuntimeAgent::getRuntimeTypesForVariablesAtOffsets(ErrorString* errorString, const RefPtr<Inspector::InspectorArray>& locations, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Runtime::TypeDescription>>& typeDescriptions)
{
    static const bool verbose = false;
    VM& vm = globalVM();
    typeDescriptions = Inspector::Protocol::Array<Inspector::Protocol::Runtime::TypeDescription>::create();
    if (!vm.typeProfiler()) {
        *errorString = ASCIILiteral("The VM does not currently have Type Information.");
        return;
    }

    double start = currentTimeMS();
    vm.typeProfilerLog()->processLogEntries(ASCIILiteral("User Query"));

    for (size_t i = 0; i < locations->length(); i++) {
        RefPtr<Inspector::InspectorValue> value = locations->get(i);
        RefPtr<InspectorObject> location;
        if (!value->asObject(location)) {
            *errorString = ASCIILiteral("Array of TypeLocation objects has an object that does not have type of TypeLocation.");
            return;
        }

        int descriptor;
        String sourceIDAsString;
        int divot;
        location->getInteger(ASCIILiteral("typeInformationDescriptor"), descriptor);
        location->getString(ASCIILiteral("sourceID"), sourceIDAsString);
        location->getInteger(ASCIILiteral("divot"), divot);

        bool okay;
        TypeLocation* typeLocation = vm.typeProfiler()->findLocation(divot, sourceIDAsString.toIntPtrStrict(&okay), static_cast<TypeProfilerSearchDescriptor>(descriptor));

        RefPtr<TypeSet> typeSet;
        if (typeLocation) {
            if (typeLocation->m_globalTypeSet && typeLocation->m_globalVariableID != TypeProfilerNoGlobalIDExists)
                typeSet = typeLocation->m_globalTypeSet;
            else
                typeSet = typeLocation->m_instructionTypeSet;
        }

        bool isValid = typeLocation && typeSet && !typeSet->isEmpty();
        RefPtr<Inspector::Protocol::Runtime::TypeDescription> description = Inspector::Protocol::Runtime::TypeDescription::create()
            .setIsValid(isValid);

        if (isValid) {
            description->setLeastCommonAncestor(typeSet->leastCommonAncestor());
            description->setPrimitiveTypeNames(typeSet->allPrimitiveTypeNames());
            description->setStructures(typeSet->allStructureRepresentations());
            description->setTypeSet(typeSet->inspectorTypeSet());
            description->setIsTruncated(typeSet->isOverflown());
        }

        typeDescriptions->addItem(description);
    }

    double end = currentTimeMS();
    if (verbose)
        dataLogF("Inspector::getRuntimeTypesForVariablesAtOffsets took %lfms\n", end - start);
}

class TypeRecompiler : public MarkedBlock::VoidFunctor {
public:
    inline void operator()(JSCell* cell)
    {
        if (!cell->inherits(FunctionExecutable::info()))
            return;

        FunctionExecutable* executable = jsCast<FunctionExecutable*>(cell);
        executable->clearCodeIfNotCompiling();
        executable->clearUnlinkedCodeForRecompilationIfNotCompiling();
    }
};

static void recompileAllJSFunctionsForTypeProfiling(VM& vm, bool shouldEnableTypeProfiling)
{
    vm.prepareToDiscardCode();

    bool needsToRecompile = (shouldEnableTypeProfiling ? vm.enableTypeProfiler() : vm.disableTypeProfiler());
    if (needsToRecompile) {
        TypeRecompiler recompiler;
        HeapIterationScope iterationScope(vm.heap);
        vm.heap.objectSpace().forEachLiveCell(iterationScope, recompiler);
    }
}

void InspectorRuntimeAgent::willDestroyFrontendAndBackend(InspectorDisconnectReason reason)
{
    if (reason != InspectorDisconnectReason::InspectedTargetDestroyed && m_isTypeProfilingEnabled)
        setTypeProfilerEnabledState(false);
}

void InspectorRuntimeAgent::enableTypeProfiler(ErrorString*)
{
    setTypeProfilerEnabledState(true);
}

void InspectorRuntimeAgent::disableTypeProfiler(ErrorString*)
{
    setTypeProfilerEnabledState(false);
}

void InspectorRuntimeAgent::setTypeProfilerEnabledState(bool shouldEnableTypeProfiling)
{
    if (m_isTypeProfilingEnabled == shouldEnableTypeProfiling)
        return;

    m_isTypeProfilingEnabled = shouldEnableTypeProfiling;

    VM& vm = globalVM();

    // If JavaScript is running, it's not safe to recompile, since we'll end
    // up throwing away code that is live on the stack.
    if (vm.entryScope) {
        vm.entryScope->setEntryScopeDidPopListener(this,
            [=] (VM& vm, JSGlobalObject*) {
                recompileAllJSFunctionsForTypeProfiling(vm, shouldEnableTypeProfiling); 
            }
        );
    } else
        recompileAllJSFunctionsForTypeProfiling(vm, shouldEnableTypeProfiling);
}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
