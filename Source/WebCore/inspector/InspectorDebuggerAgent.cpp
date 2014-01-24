/*
 * Copyright (C) 2010, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010-2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorDebuggerAgent.h"

#if ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)

#include "CachedResource.h"
#include "ContentSearchUtils.h"
#include "InspectorPageAgent.h"
#include "InstrumentingAgents.h"
#include "RegularExpression.h"
#include "ScriptDebugServer.h"
#include <bindings/ScriptObject.h>
#include <bindings/ScriptValue.h>
#include <inspector/InjectedScript.h>
#include <inspector/InjectedScriptManager.h>
#include <inspector/InspectorValues.h>
#include <wtf/text/WTFString.h>

using Inspector::TypeBuilder::Array;
using Inspector::TypeBuilder::Debugger::FunctionDetails;
using Inspector::TypeBuilder::Debugger::ScriptId;
using Inspector::TypeBuilder::Runtime::RemoteObject;

using namespace Inspector;

namespace WebCore {

const char* InspectorDebuggerAgent::backtraceObjectGroup = "backtrace";

static String objectGroupForBreakpointAction(int identifier)
{
    DEFINE_STATIC_LOCAL(const AtomicString, objectGroup, ("breakpoint-action-", AtomicString::ConstructFromLiteral));
    return makeString(objectGroup, String::number(identifier));
}

InspectorDebuggerAgent::InspectorDebuggerAgent(InstrumentingAgents* instrumentingAgents, InjectedScriptManager* injectedScriptManager)
    : InspectorAgentBase(ASCIILiteral("Debugger"), instrumentingAgents)
    , m_injectedScriptManager(injectedScriptManager)
    , m_pausedScriptState(nullptr)
    , m_continueToLocationBreakpointID(JSC::noBreakpointID)
    , m_enabled(false)
    , m_javaScriptPauseScheduled(false)
    , m_listener(nullptr)
    , m_nextProbeSampleId(1)
    , m_nextBreakpointActionIdentifier(1)
{
    // FIXME: make breakReason optional so that there was no need to init it with "other".
    clearBreakDetails();
}

InspectorDebuggerAgent::~InspectorDebuggerAgent()
{
    ASSERT(!m_instrumentingAgents->inspectorDebuggerAgent());
}

void InspectorDebuggerAgent::enable()
{
    m_instrumentingAgents->setInspectorDebuggerAgent(this);

    scriptDebugServer().setBreakpointsActivated(true);
    startListeningScriptDebugServer();

    if (m_listener)
        m_listener->debuggerWasEnabled();

    m_enabled = true;
}

void InspectorDebuggerAgent::disable(bool isBeingDestroyed)
{
    m_javaScriptBreakpoints.clear();
    m_instrumentingAgents->setInspectorDebuggerAgent(nullptr);

    stopListeningScriptDebugServer(isBeingDestroyed);
    clearResolvedBreakpointState();

    if (m_listener)
        m_listener->debuggerWasDisabled();

    m_enabled = false;
}

void InspectorDebuggerAgent::enable(ErrorString*)
{
    if (m_enabled)
        return;

    enable();

    ASSERT(m_frontendDispatcher);
}

void InspectorDebuggerAgent::disable(ErrorString*)
{
    if (!m_enabled)
        return;

    bool skipRecompile = false;
    disable(skipRecompile);
}

void InspectorDebuggerAgent::didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel* frontendChannel, InspectorBackendDispatcher* backendDispatcher)
{
    m_frontendDispatcher = std::make_unique<InspectorDebuggerFrontendDispatcher>(frontendChannel);
    m_backendDispatcher = InspectorDebuggerBackendDispatcher::create(backendDispatcher, this);
}

void InspectorDebuggerAgent::willDestroyFrontendAndBackend(InspectorDisconnectReason reason)
{
    m_frontendDispatcher = nullptr;
    m_backendDispatcher.clear();

    bool skipRecompile = reason == InspectorDisconnectReason::InspectedTargetDestroyed;
    disable(skipRecompile);
}

void InspectorDebuggerAgent::setBreakpointsActive(ErrorString*, bool active)
{
    if (active)
        scriptDebugServer().activateBreakpoints();
    else
        scriptDebugServer().deactivateBreakpoints();
}

bool InspectorDebuggerAgent::isPaused()
{
    return scriptDebugServer().isPaused();
}

void InspectorDebuggerAgent::addMessageToConsole(MessageSource source, MessageType type)
{
    if (scriptDebugServer().pauseOnExceptionsState() != ScriptDebugServer::DontPauseOnExceptions && source == ConsoleAPIMessageSource && type == AssertMessageType)
        breakProgram(InspectorDebuggerFrontendDispatcher::Reason::Assert, nullptr);
}

static PassRefPtr<InspectorObject> buildObjectForBreakpointCookie(const String& url, int lineNumber, int columnNumber, const String& condition, RefPtr<InspectorArray>& actions, bool isRegex, bool autoContinue)
{
    RefPtr<InspectorObject> breakpointObject = InspectorObject::create();
    breakpointObject->setString("url", url);
    breakpointObject->setNumber("lineNumber", lineNumber);
    breakpointObject->setNumber("columnNumber", columnNumber);
    breakpointObject->setString("condition", condition);
    breakpointObject->setBoolean("isRegex", isRegex);
    breakpointObject->setBoolean("autoContinue", autoContinue);
    if (actions)
        breakpointObject->setArray("actions", actions);

    return breakpointObject;
}

static bool matches(const String& url, const String& pattern, bool isRegex)
{
    if (isRegex) {
        RegularExpression regex(pattern, TextCaseSensitive);
        return regex.match(url) != -1;
    }
    return url == pattern;
}

static bool breakpointActionTypeForString(const String& typeString, ScriptBreakpointActionType* output)
{
    if (typeString == Inspector::TypeBuilder::getJSEnumConstantValue(Inspector::TypeBuilder::Debugger::BreakpointAction::Type::Log)) {
        *output = ScriptBreakpointActionTypeLog;
        return true;
    }
    if (typeString == Inspector::TypeBuilder::getJSEnumConstantValue(Inspector::TypeBuilder::Debugger::BreakpointAction::Type::Evaluate)) {
        *output = ScriptBreakpointActionTypeEvaluate;
        return true;
    }
    if (typeString == Inspector::TypeBuilder::getJSEnumConstantValue(Inspector::TypeBuilder::Debugger::BreakpointAction::Type::Sound)) {
        *output = ScriptBreakpointActionTypeSound;
        return true;
    }
    if (typeString == Inspector::TypeBuilder::getJSEnumConstantValue(Inspector::TypeBuilder::Debugger::BreakpointAction::Type::Probe)) {
        *output = ScriptBreakpointActionTypeProbe;
        return true;
    }

    return false;
}

bool InspectorDebuggerAgent::breakpointActionsFromProtocol(ErrorString* errorString, RefPtr<InspectorArray>& actions, Vector<ScriptBreakpointAction>* result)
{
    if (!actions)
        return true;

    unsigned actionsLength = actions->length();
    if (!actionsLength)
        return true;

    result->reserveCapacity(actionsLength);
    for (unsigned i = 0; i < actionsLength; ++i) {
        RefPtr<InspectorValue> value = actions->get(i);
        RefPtr<InspectorObject> object;
        if (!value->asObject(&object)) {
            *errorString = "BreakpointAction of incorrect type, expected object";
            return false;
        }

        String typeString;
        if (!object->getString("type", &typeString)) {
            *errorString = "BreakpointAction had type missing";
            return false;
        }

        ScriptBreakpointActionType type;
        if (!breakpointActionTypeForString(typeString, &type)) {
            *errorString = "BreakpointAction had unknown type";
            return false;
        }

        String data;
        object->getString("data", &data);

        result->append(ScriptBreakpointAction(type, m_nextBreakpointActionIdentifier++, data));
    }

    return true;
}

void InspectorDebuggerAgent::setBreakpointByUrl(ErrorString* errorString, int lineNumber, const String* const optionalURL, const String* const optionalURLRegex, const int* const optionalColumnNumber, const RefPtr<InspectorObject>* options, Inspector::TypeBuilder::Debugger::BreakpointId* outBreakpointIdentifier, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Debugger::Location>>& locations, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Debugger::BreakpointActionIdentifier>>& breakpointActionIdentifiers)
{
    locations = Array<Inspector::TypeBuilder::Debugger::Location>::create();
    if (!optionalURL == !optionalURLRegex) {
        *errorString = "Either url or urlRegex must be specified.";
        return;
    }

    String url = optionalURL ? *optionalURL : *optionalURLRegex;
    int columnNumber = optionalColumnNumber ? *optionalColumnNumber : 0;
    bool isRegex = optionalURLRegex;

    String breakpointIdentifier = (isRegex ? "/" + url + "/" : url) + ':' + String::number(lineNumber) + ':' + String::number(columnNumber);
    if (m_javaScriptBreakpoints.contains(breakpointIdentifier)) {
        *errorString = "Breakpoint at specified location already exists.";
        return;
    }

    String condition = emptyString();
    bool autoContinue = false;
    RefPtr<InspectorArray> actions;
    if (options) {
        (*options)->getString("condition", &condition);
        (*options)->getBoolean("autoContinue", &autoContinue);
        actions = (*options)->getArray("actions");
    }

    Vector<ScriptBreakpointAction> breakpointActions;
    if (!breakpointActionsFromProtocol(errorString, actions, &breakpointActions))
        return;

    breakpointActionIdentifiers = Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Debugger::BreakpointActionIdentifier>::create();
    for (ScriptBreakpointAction& action : breakpointActions)
        breakpointActionIdentifiers->addItem(action.identifier);

    m_javaScriptBreakpoints.set(breakpointIdentifier, buildObjectForBreakpointCookie(url, lineNumber, columnNumber, condition, actions, isRegex, autoContinue));

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition, breakpointActions, autoContinue);
    for (ScriptsMap::iterator it = m_scripts.begin(); it != m_scripts.end(); ++it) {
        String scriptURL = !it->value.sourceURL.isEmpty() ? it->value.sourceURL : it->value.url;
        if (!matches(scriptURL, url, isRegex))
            continue;

        RefPtr<Inspector::TypeBuilder::Debugger::Location> location = resolveBreakpoint(breakpointIdentifier, it->key, breakpoint);
        if (location)
            locations->addItem(location);
    }
    *outBreakpointIdentifier = breakpointIdentifier;
}

static bool parseLocation(ErrorString* errorString, InspectorObject* location, JSC::SourceID* sourceID, unsigned* lineNumber, unsigned* columnNumber)
{
    String scriptIDStr;
    if (!location->getString("scriptId", &scriptIDStr) || !location->getNumber("lineNumber", lineNumber)) {
        // FIXME: replace with input validation.
        *sourceID = JSC::noSourceID;
        *errorString = "scriptId and lineNumber are required.";
        return false;
    }
    *sourceID = scriptIDStr.toIntPtr();
    *columnNumber = 0;
    location->getNumber("columnNumber", columnNumber);
    return true;
}

void InspectorDebuggerAgent::setBreakpoint(ErrorString* errorString, const RefPtr<InspectorObject>& location, const RefPtr<InspectorObject>* options, Inspector::TypeBuilder::Debugger::BreakpointId* outBreakpointIdentifier, RefPtr<Inspector::TypeBuilder::Debugger::Location>& actualLocation, RefPtr<Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Debugger::BreakpointActionIdentifier>>& breakpointActionIdentifiers)
{
    JSC::SourceID sourceID;
    unsigned lineNumber;
    unsigned columnNumber;

    if (!parseLocation(errorString, location.get(), &sourceID, &lineNumber, &columnNumber))
        return;

    String condition = emptyString();
    bool autoContinue = false;
    RefPtr<InspectorArray> actions;
    if (options) {
        (*options)->getString("condition", &condition);
        (*options)->getBoolean("autoContinue", &autoContinue);
        actions = (*options)->getArray("actions");
    }

    Vector<ScriptBreakpointAction> breakpointActions;
    if (!breakpointActionsFromProtocol(errorString, actions, &breakpointActions))
        return;
    
    breakpointActionIdentifiers = Inspector::TypeBuilder::Array<Inspector::TypeBuilder::Debugger::BreakpointActionIdentifier>::create();
    for (ScriptBreakpointAction& action : breakpointActions)
        breakpointActionIdentifiers->addItem(action.identifier);

    String breakpointIdentifier = String::number(sourceID) + ':' + String::number(lineNumber) + ':' + String::number(columnNumber);
    if (m_breakpointIdentifierToDebugServerBreakpointIDs.find(breakpointIdentifier) != m_breakpointIdentifierToDebugServerBreakpointIDs.end()) {
        *errorString = "Breakpoint at specified location already exists.";
        return;
    }

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, condition, breakpointActions, autoContinue);
    actualLocation = resolveBreakpoint(breakpointIdentifier, sourceID, breakpoint);
    if (actualLocation)
        *outBreakpointIdentifier = breakpointIdentifier;
    else
        *errorString = "Could not resolve breakpoint";
}

void InspectorDebuggerAgent::removeBreakpoint(ErrorString*, const String& breakpointIdentifier)
{
    m_javaScriptBreakpoints.remove(breakpointIdentifier);

    Vector<JSC::BreakpointID> breakpointIDs = m_breakpointIdentifierToDebugServerBreakpointIDs.take(breakpointIdentifier);
    for (auto breakpointID : breakpointIDs) {
        const Vector<ScriptBreakpointAction>& breakpointActions = scriptDebugServer().getActionsForBreakpoint(breakpointID);
        for (auto& action : breakpointActions)
            m_injectedScriptManager->releaseObjectGroup(objectGroupForBreakpointAction(action.identifier));

        scriptDebugServer().removeBreakpoint(breakpointID);
    }
}

void InspectorDebuggerAgent::continueToLocation(ErrorString* errorString, const RefPtr<InspectorObject>& location)
{
    if (m_continueToLocationBreakpointID != JSC::noBreakpointID) {
        scriptDebugServer().removeBreakpoint(m_continueToLocationBreakpointID);
        m_continueToLocationBreakpointID = JSC::noBreakpointID;
    }

    JSC::SourceID sourceID;
    unsigned lineNumber;
    unsigned columnNumber;

    if (!parseLocation(errorString, location.get(), &sourceID, &lineNumber, &columnNumber))
        return;

    ScriptBreakpoint breakpoint(lineNumber, columnNumber, "", false);
    m_continueToLocationBreakpointID = scriptDebugServer().setBreakpoint(sourceID, breakpoint, &lineNumber, &columnNumber);
    resume(errorString);
}

PassRefPtr<Inspector::TypeBuilder::Debugger::Location> InspectorDebuggerAgent::resolveBreakpoint(const String& breakpointIdentifier, JSC::SourceID sourceID, const ScriptBreakpoint& breakpoint)
{
    ScriptsMap::iterator scriptIterator = m_scripts.find(sourceID);
    if (scriptIterator == m_scripts.end())
        return nullptr;
    Script& script = scriptIterator->value;
    if (breakpoint.lineNumber < script.startLine || script.endLine < breakpoint.lineNumber)
        return nullptr;

    unsigned actualLineNumber;
    unsigned actualColumnNumber;
    JSC::BreakpointID debugServerBreakpointID = scriptDebugServer().setBreakpoint(sourceID, breakpoint, &actualLineNumber, &actualColumnNumber);
    if (debugServerBreakpointID == JSC::noBreakpointID)
        return nullptr;

    BreakpointIdentifierToDebugServerBreakpointIDsMap::iterator debugServerBreakpointIDsIterator = m_breakpointIdentifierToDebugServerBreakpointIDs.find(breakpointIdentifier);
    if (debugServerBreakpointIDsIterator == m_breakpointIdentifierToDebugServerBreakpointIDs.end())
        debugServerBreakpointIDsIterator = m_breakpointIdentifierToDebugServerBreakpointIDs.set(breakpointIdentifier, Vector<JSC::BreakpointID>()).iterator;
    debugServerBreakpointIDsIterator->value.append(debugServerBreakpointID);

    RefPtr<Inspector::TypeBuilder::Debugger::Location> location = Inspector::TypeBuilder::Debugger::Location::create()
        .setScriptId(String::number(sourceID))
        .setLineNumber(actualLineNumber);
    location->setColumnNumber(actualColumnNumber);
    return location;
}

void InspectorDebuggerAgent::searchInContent(ErrorString* error, const String& scriptIDStr, const String& query, const bool* const optionalCaseSensitive, const bool* const optionalIsRegex, RefPtr<Array<Inspector::TypeBuilder::GenericTypes::SearchMatch>>& results)
{
    bool isRegex = optionalIsRegex ? *optionalIsRegex : false;
    bool caseSensitive = optionalCaseSensitive ? *optionalCaseSensitive : false;

    JSC::SourceID sourceID = scriptIDStr.toIntPtr();
    ScriptsMap::iterator it = m_scripts.find(sourceID);
    if (it != m_scripts.end())
        results = ContentSearchUtils::searchInTextByLines(it->value.source, query, caseSensitive, isRegex);
    else
        *error = "No script for id: " + scriptIDStr;
}

void InspectorDebuggerAgent::getScriptSource(ErrorString* error, const String& scriptIDStr, String* scriptSource)
{
    JSC::SourceID sourceID = scriptIDStr.toIntPtr();
    ScriptsMap::iterator it = m_scripts.find(sourceID);
    if (it != m_scripts.end())
        *scriptSource = it->value.source;
    else
        *error = "No script for id: " + scriptIDStr;
}

void InspectorDebuggerAgent::getFunctionDetails(ErrorString* errorString, const String& functionId, RefPtr<Inspector::TypeBuilder::Debugger::FunctionDetails>& details)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(functionId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Function object id is obsolete";
        return;
    }
    injectedScript.getFunctionDetails(errorString, functionId, &details);
}

void InspectorDebuggerAgent::schedulePauseOnNextStatement(InspectorDebuggerFrontendDispatcher::Reason::Enum breakReason, PassRefPtr<InspectorObject> data)
{
    if (m_javaScriptPauseScheduled)
        return;
    m_breakReason = breakReason;
    m_breakAuxData = data;
    scriptDebugServer().setPauseOnNextStatement(true);
}

void InspectorDebuggerAgent::cancelPauseOnNextStatement()
{
    if (m_javaScriptPauseScheduled)
        return;
    clearBreakDetails();
    scriptDebugServer().setPauseOnNextStatement(false);
}

void InspectorDebuggerAgent::pause(ErrorString*)
{
    if (m_javaScriptPauseScheduled)
        return;
    clearBreakDetails();
    scriptDebugServer().setPauseOnNextStatement(true);
    m_javaScriptPauseScheduled = true;
}

void InspectorDebuggerAgent::resume(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().continueProgram();
}

void InspectorDebuggerAgent::stepOver(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().stepOverStatement();
}

void InspectorDebuggerAgent::stepInto(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().stepIntoStatement();
    m_listener->stepInto();
}

void InspectorDebuggerAgent::stepOut(ErrorString* errorString)
{
    if (!assertPaused(errorString))
        return;
    m_injectedScriptManager->releaseObjectGroup(InspectorDebuggerAgent::backtraceObjectGroup);
    scriptDebugServer().stepOutOfFunction();
}

void InspectorDebuggerAgent::setPauseOnExceptions(ErrorString* errorString, const String& stringPauseState)
{
    ScriptDebugServer::PauseOnExceptionsState pauseState;
    if (stringPauseState == "none")
        pauseState = ScriptDebugServer::DontPauseOnExceptions;
    else if (stringPauseState == "all")
        pauseState = ScriptDebugServer::PauseOnAllExceptions;
    else if (stringPauseState == "uncaught")
        pauseState = ScriptDebugServer::PauseOnUncaughtExceptions;
    else {
        *errorString = "Unknown pause on exceptions mode: " + stringPauseState;
        return;
    }

    scriptDebugServer().setPauseOnExceptionsState(static_cast<ScriptDebugServer::PauseOnExceptionsState>(pauseState));
    if (scriptDebugServer().pauseOnExceptionsState() != pauseState)
        *errorString = "Internal error. Could not change pause on exceptions state";
}

void InspectorDebuggerAgent::evaluateOnCallFrame(ErrorString* errorString, const String& callFrameId, const String& expression, const String* const objectGroup, const bool* const includeCommandLineAPI, const bool* const doNotPauseOnExceptionsAndMuteConsole, const bool* const returnByValue, const bool* generatePreview, RefPtr<Inspector::TypeBuilder::Runtime::RemoteObject>& result, Inspector::TypeBuilder::OptOutput<bool>* wasThrown)
{
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptForObjectId(callFrameId);
    if (injectedScript.hasNoValue()) {
        *errorString = "Inspected frame has gone";
        return;
    }

    ScriptDebugServer::PauseOnExceptionsState previousPauseOnExceptionsState = scriptDebugServer().pauseOnExceptionsState();
    if (doNotPauseOnExceptionsAndMuteConsole ? *doNotPauseOnExceptionsAndMuteConsole : false) {
        if (previousPauseOnExceptionsState != ScriptDebugServer::DontPauseOnExceptions)
            scriptDebugServer().setPauseOnExceptionsState(ScriptDebugServer::DontPauseOnExceptions);
        muteConsole();
    }

    injectedScript.evaluateOnCallFrame(errorString, m_currentCallStack, callFrameId, expression, objectGroup ? *objectGroup : "", includeCommandLineAPI ? *includeCommandLineAPI : false, returnByValue ? *returnByValue : false, generatePreview ? *generatePreview : false, &result, wasThrown);

    if (doNotPauseOnExceptionsAndMuteConsole ? *doNotPauseOnExceptionsAndMuteConsole : false) {
        unmuteConsole();
        if (scriptDebugServer().pauseOnExceptionsState() != previousPauseOnExceptionsState)
            scriptDebugServer().setPauseOnExceptionsState(previousPauseOnExceptionsState);
    }
}

void InspectorDebuggerAgent::setOverlayMessage(ErrorString*, const String*)
{
}

void InspectorDebuggerAgent::scriptExecutionBlockedByCSP(const String& directiveText)
{
    if (scriptDebugServer().pauseOnExceptionsState() != ScriptDebugServer::DontPauseOnExceptions) {
        RefPtr<InspectorObject> directive = InspectorObject::create();
        directive->setString("directiveText", directiveText);
        breakProgram(InspectorDebuggerFrontendDispatcher::Reason::CSPViolation, directive.release());
    }
}

PassRefPtr<Array<Inspector::TypeBuilder::Debugger::CallFrame>> InspectorDebuggerAgent::currentCallFrames()
{
    if (!m_pausedScriptState)
        return Array<Inspector::TypeBuilder::Debugger::CallFrame>::create();
    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(m_pausedScriptState);
    if (injectedScript.hasNoValue()) {
        ASSERT_NOT_REACHED();
        return Array<Inspector::TypeBuilder::Debugger::CallFrame>::create();
    }
    return injectedScript.wrapCallFrames(m_currentCallStack);
}

String InspectorDebuggerAgent::sourceMapURLForScript(const Script& script)
{
    DEFINE_STATIC_LOCAL(String, sourceMapHTTPHeader, (ASCIILiteral("SourceMap")));
    DEFINE_STATIC_LOCAL(String, sourceMapHTTPHeaderDeprecated, (ASCIILiteral("X-SourceMap")));

    if (!script.url.isEmpty()) {
        if (InspectorPageAgent* pageAgent = m_instrumentingAgents->inspectorPageAgent()) {
            CachedResource* resource = pageAgent->cachedResource(pageAgent->mainFrame(), URL(ParsedURLString, script.url));
            if (resource) {
                String sourceMapHeader = resource->response().httpHeaderField(sourceMapHTTPHeader);
                if (!sourceMapHeader.isEmpty())
                    return sourceMapHeader;

                sourceMapHeader = resource->response().httpHeaderField(sourceMapHTTPHeaderDeprecated);
                if (!sourceMapHeader.isEmpty())
                    return sourceMapHeader;
            }
        }
    }

    return ContentSearchUtils::findScriptSourceMapURL(script.source);
}

// JavaScriptDebugListener functions

void InspectorDebuggerAgent::didParseSource(JSC::SourceID sourceID, const Script& inScript)
{
    Script script = inScript;
    if (!script.startLine && !script.startColumn)
        script.sourceURL = ContentSearchUtils::findScriptSourceURL(script.source);
    script.sourceMappingURL = sourceMapURLForScript(script);

    bool hasSourceURL = !script.sourceURL.isEmpty();
    String scriptURL = hasSourceURL ? script.sourceURL : script.url;
    bool* hasSourceURLParam = hasSourceURL ? &hasSourceURL : nullptr;
    String* sourceMapURLParam = script.sourceMappingURL.isNull() ? nullptr : &script.sourceMappingURL;
    const bool* isContentScript = script.isContentScript ? &script.isContentScript : nullptr;
    String scriptIDStr = String::number(sourceID);
    m_frontendDispatcher->scriptParsed(scriptIDStr, scriptURL, script.startLine, script.startColumn, script.endLine, script.endColumn, isContentScript, sourceMapURLParam, hasSourceURLParam);

    m_scripts.set(sourceID, script);

    if (scriptURL.isEmpty())
        return;

    for (auto it = m_javaScriptBreakpoints.begin(), end = m_javaScriptBreakpoints.end(); it != end; ++it) {
        RefPtr<InspectorObject> breakpointObject = it->value->asObject();
        bool isRegex;
        breakpointObject->getBoolean("isRegex", &isRegex);
        String url;
        breakpointObject->getString("url", &url);
        if (!matches(scriptURL, url, isRegex))
            continue;
        ScriptBreakpoint breakpoint;
        breakpointObject->getNumber("lineNumber", &breakpoint.lineNumber);
        breakpointObject->getNumber("columnNumber", &breakpoint.columnNumber);
        breakpointObject->getString("condition", &breakpoint.condition);
        breakpointObject->getBoolean("autoContinue", &breakpoint.autoContinue);
        ErrorString errorString;
        RefPtr<InspectorArray> actions = breakpointObject->getArray("actions");
        if (!breakpointActionsFromProtocol(&errorString, actions, &breakpoint.actions)) {
            ASSERT_NOT_REACHED();
            continue;
        }

        RefPtr<Inspector::TypeBuilder::Debugger::Location> location = resolveBreakpoint(it->key, sourceID, breakpoint);
        if (location)
            m_frontendDispatcher->breakpointResolved(it->key, location);
    }
}

void InspectorDebuggerAgent::failedToParseSource(const String& url, const String& data, int firstLine, int errorLine, const String& errorMessage)
{
    m_frontendDispatcher->scriptFailedToParse(url, data, firstLine, errorLine, errorMessage);
}

void InspectorDebuggerAgent::didPause(JSC::ExecState* scriptState, const Deprecated::ScriptValue& callFrames, const Deprecated::ScriptValue& exception)
{
    ASSERT(scriptState && !m_pausedScriptState);
    m_pausedScriptState = scriptState;
    m_currentCallStack = callFrames;

    if (!exception.hasNoValue()) {
        InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(scriptState);
        if (!injectedScript.hasNoValue()) {
            m_breakReason = InspectorDebuggerFrontendDispatcher::Reason::Exception;
            m_breakAuxData = injectedScript.wrapObject(exception, "backtrace")->openAccessors();
            // m_breakAuxData might be null after this.
        }
    }

    m_frontendDispatcher->paused(currentCallFrames(), m_breakReason, m_breakAuxData);
    m_javaScriptPauseScheduled = false;

    if (m_continueToLocationBreakpointID != JSC::noBreakpointID) {
        scriptDebugServer().removeBreakpoint(m_continueToLocationBreakpointID);
        m_continueToLocationBreakpointID = JSC::noBreakpointID;
    }
    if (m_listener)
        m_listener->didPause();
}

void InspectorDebuggerAgent::didSampleProbe(JSC::ExecState* scriptState, int probeIdentifier, int hitCount, const Deprecated::ScriptValue& sample)
{
    int sampleId = m_nextProbeSampleId++;

    InjectedScript injectedScript = m_injectedScriptManager->injectedScriptFor(scriptState);
    RefPtr<TypeBuilder::Runtime::RemoteObject> payload = injectedScript.wrapObject(sample, objectGroupForBreakpointAction(probeIdentifier));
    RefPtr<TypeBuilder::Debugger::ProbeSample> result = TypeBuilder::Debugger::ProbeSample::create()
        .setProbeId(probeIdentifier)
        .setSampleId(sampleId)
        .setBatchId(hitCount)
        .setTimestamp(monotonicallyIncreasingTime())
        .setPayload(payload.release());

    m_frontendDispatcher->didSampleProbe(result.release());
}

void InspectorDebuggerAgent::didContinue()
{
    m_pausedScriptState = nullptr;
    m_currentCallStack = Deprecated::ScriptValue();
    clearBreakDetails();
    m_frontendDispatcher->resumed();
}

void InspectorDebuggerAgent::breakProgram(InspectorDebuggerFrontendDispatcher::Reason::Enum breakReason, PassRefPtr<InspectorObject> data)
{
    m_breakReason = breakReason;
    m_breakAuxData = data;
    scriptDebugServer().breakProgram();
}

void InspectorDebuggerAgent::clearResolvedBreakpointState()
{
    ErrorString dummyError;
    Vector<String> breakpointIdentifiers;
    copyKeysToVector(m_breakpointIdentifierToDebugServerBreakpointIDs, breakpointIdentifiers);
    for (const String& identifier : breakpointIdentifiers)
        removeBreakpoint(&dummyError, identifier);

    scriptDebugServer().continueProgram();

    m_pausedScriptState = nullptr;
    m_currentCallStack = Deprecated::ScriptValue();
    m_scripts.clear();
    m_breakpointIdentifierToDebugServerBreakpointIDs.clear();
    m_continueToLocationBreakpointID = JSC::noBreakpointID;
    clearBreakDetails();
    m_javaScriptPauseScheduled = false;
    setOverlayMessage(&dummyError, nullptr);
}

bool InspectorDebuggerAgent::assertPaused(ErrorString* errorString)
{
    if (!m_pausedScriptState) {
        *errorString = "Can only perform operation while paused.";
        return false;
    }
    return true;
}

void InspectorDebuggerAgent::clearBreakDetails()
{
    m_breakReason = InspectorDebuggerFrontendDispatcher::Reason::Other;
    m_breakAuxData = nullptr;
}

void InspectorDebuggerAgent::didClearGlobalObject()
{
    if (m_frontendDispatcher)
        m_frontendDispatcher->globalObjectCleared();

    clearResolvedBreakpointState();
}

} // namespace WebCore

#endif // ENABLE(JAVASCRIPT_DEBUGGER) && ENABLE(INSPECTOR)
