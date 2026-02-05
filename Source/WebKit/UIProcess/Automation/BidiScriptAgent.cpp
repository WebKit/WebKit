/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Microsoft Corporation. All rights reserved.
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

#include "config.h"
#include "BidiScriptAgent.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "AutomationProtocolObjects.h"
#include "FrameTreeNodeData.h"
#include "PageLoadState.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProtocolObjects.h"
#include "WebFrameMetrics.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <algorithm>
#include <wtf/CallbackAggregator.h>
#include <wtf/ProcessID.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebKit {

using namespace Inspector;
using BrowsingContext = Inspector::Protocol::BidiBrowsingContext::BrowsingContext;
using EvaluateResultType = Inspector::Protocol::BidiScript::EvaluateResultType;

static RefPtr<Inspector::Protocol::BidiScript::RemoteValue> deserializeRemoteValue(const JSON::Value*);

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiScriptAgent);

BidiScriptAgent::BidiScriptAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_scriptDomainDispatcher(BidiScriptBackendDispatcher::create(backendDispatcher, this))
{
}

BidiScriptAgent::~BidiScriptAgent() = default;

static RefPtr<Inspector::Protocol::BidiScript::RemoteValue> deserializeRemoteValue(const JSON::Value* jsonValue)
{
    // FIXME: Implement full BiDi RemoteValue deserialization (array, object, map, set, etc.)
    // https://bugs.webkit.org/show_bug.cgi?id=288060
    using RemoteValue = Inspector::Protocol::BidiScript::RemoteValue;
    using RemoteValueType = Inspector::Protocol::BidiScript::RemoteValueType;

    auto resultValue = RemoteValue::create();
    auto object = jsonValue ? jsonValue->asObject() : nullptr;
    if (!object)
        return resultValue.setType(RemoteValueType::Undefined).release();

    String typeString = object->getString("type"_s);

    // Primitive types with values.
    if (typeString == "string"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::String).release();
        remoteValue->setValue(JSON::Value::create(object->getString("value"_s)));
        return remoteValue;
    }
    if (typeString == "number"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::Number).release();
        if (auto num = object->getDouble("value"_s))
            remoteValue->setValue(JSON::Value::create(*num));
        else
            remoteValue->setValue(JSON::Value::create(object->getString("value"_s)));
        return remoteValue;
    }
    if (typeString == "boolean"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::Boolean).release();
        if (auto b = object->getBoolean("value"_s))
            remoteValue->setValue(JSON::Value::create(*b));
        return remoteValue;
    }
    if (typeString == "bigint"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::Bigint).release();
        remoteValue->setValue(JSON::Value::create(object->getString("value"_s)));
        return remoteValue;
    }

    // Primitive types without values.
    if (typeString == "null"_s)
        return resultValue.setType(RemoteValueType::Null).release();
    if (typeString == "symbol"_s)
        return resultValue.setType(RemoteValueType::Symbol).release();
    if (typeString == "function"_s)
        return resultValue.setType(RemoteValueType::Function).release();

    // Simple structured types.
    if (typeString == "date"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::Date).release();
        remoteValue->setValue(JSON::Value::create(object->getString("value"_s)));
        return remoteValue;
    }
    if (typeString == "regexp"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::Regexp).release();
        if (auto value = object->getValue("value"_s))
            remoteValue->setValue(value.releaseNonNull());
        return remoteValue;
    }

    // Other types that serialize without values.
    if (typeString == "promise"_s)
        return resultValue.setType(RemoteValueType::Promise).release();
    if (typeString == "error"_s)
        return resultValue.setType(RemoteValueType::Error).release();

    // For unknown/unhandled types, pass through the original JSON structure.
    // This allows complex types like iterators, objects, arrays, etc. to be preserved
    // even if we don't explicitly deserialize them yet.
    if (!typeString.isNull()) {
        // Try to match known type strings to enum values.
        if (typeString == "object"_s) {
            auto remoteValue = resultValue.setType(RemoteValueType::Object).release();
            if (auto value = object->getValue("value"_s))
                remoteValue->setValue(value.releaseNonNull());
            return remoteValue;
        }
        // For any other unknown types, default to undefined.
    }

    return resultValue.setType(RemoteValueType::Undefined).release();
}

void BidiScriptAgent::callFunction(const String& functionDeclaration, bool awaitPromise, Ref<JSON::Object>&& target, RefPtr<JSON::Array>&& arguments, std::optional<Inspector::Protocol::BidiScript::ResultOwnership>&&, RefPtr<JSON::Object>&& optionalSerializationOptions, RefPtr<JSON::Object>&& optionalThis, std::optional<bool>&& optionalUserActivation, CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // FIXME: handle non-BrowsingContext obtained from `Target`.
    std::optional<BrowsingContext> browsingContext = target->getString("context"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!browsingContext, InvalidParameter);

    auto pageAndFrameHandles = session->extractBrowsingContextHandles(*browsingContext);
    ASYNC_FAIL_IF_UNEXPECTED_RESULT(pageAndFrameHandles);
    auto& [topLevelContextHandle, frameHandle] = pageAndFrameHandles.value();

    // FIXME: handle `awaitPromise` option.
    // FIXME: handle `resultOwnership` option.
    // FIXME: handle `serializationOptions` option.
    // FIXME: handle custom `this` option.
    // FIXME: handle `userActivation` option.

    Ref<JSON::Array> argumentsArray = arguments ? arguments.releaseNonNull() : JSON::Array::create();

    String realmID = generateRealmIdForBrowsingContext(*browsingContext);
    session->evaluateJavaScriptFunction(topLevelContextHandle, frameHandle, functionDeclaration, WTF::move(argumentsArray), false, optionalUserActivation.value_or(false), std::nullopt, [callback = WTF::move(callback), realmID](Inspector::CommandResult<String>&& stringResult) {
        // FIXME: Properly fill ExceptionDetails remaining fields once we have a way to get them instead of just the error message.
        // https://bugs.webkit.org/show_bug.cgi?id=288058
        if (!stringResult) {
            if (stringResult.error().startsWith("JavaScriptError"_s)) {
                String errorMessage = stringResult.error().right("JavaScriptError;"_s.length());
                // Construct error object structure for RemoteValue.value per BiDi spec.
                auto errorObject = JSON::Object::create();
                errorObject->setString("message"_s, errorMessage);
                auto exceptionValue = Inspector::Protocol::BidiScript::RemoteValue::create()
                    .setType(Inspector::Protocol::BidiScript::RemoteValueType::Error)
                    .release();
                exceptionValue->setValue(WTF::move(errorObject));
                auto stackTrace = Inspector::Protocol::BidiScript::StackTrace::create()
                    .setCallFrames(JSON::ArrayOf<Inspector::Protocol::BidiScript::StackFrame>::create())
                    .release();
                auto exceptionDetails = Inspector::Protocol::BidiScript::ExceptionDetails::create()
                    .setText(errorMessage)
                    .setLineNumber(0)
                    .setColumnNumber(0)
                    .setException(WTF::move(exceptionValue))
                    .setStackTrace(WTF::move(stackTrace))
                    .release();

                callback({ { EvaluateResultType::Exception, realmID, nullptr, WTF::move(exceptionDetails) } });
                return;
            }

            callback(makeUnexpected(stringResult.error()));
            return;
        }

        auto resultValue = JSON::Value::parseJSON(stringResult.value());
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!resultValue, InternalError, "Failed to parse callFunction result as JSON"_s);

        auto resultObject = Inspector::Protocol::BidiScript::RemoteValue::create()
            .setType(Inspector::Protocol::BidiScript::RemoteValueType::Object)
            .release();

        resultObject->setValue(resultValue.releaseNonNull());

        callback({ { EvaluateResultType::Success, realmID, WTF::move(resultObject), nullptr } });
    });
}

void BidiScriptAgent::evaluate(const String& expression, bool awaitPromise, Ref<JSON::Object>&& target, std::optional<Inspector::Protocol::BidiScript::ResultOwnership>&&, RefPtr<JSON::Object>&&, std::optional<bool>&&, CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // FIXME: Add full parameter validation (resultOwnership, serializationOptions, sandbox, realm targets, etc.)
    // https://bugs.webkit.org/show_bug.cgi?id=288060
    std::optional<BrowsingContext> browsingContext = target->getString("context"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!browsingContext, InvalidParameter);

    auto pageAndFrameHandles = session->extractBrowsingContextHandles(*browsingContext);
    ASYNC_FAIL_IF_UNEXPECTED_RESULT(pageAndFrameHandles);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session->webPageProxyForHandle(*browsingContext), FrameNotFound);

    String realmId = m_realmRegistry.realmIdForContext(*browsingContext);

    session->evaluateBidiScript(*browsingContext, emptyString(), expression, awaitPromise, 1, std::nullopt,
        [weakThis = WeakPtr { *this }, callback = WTF::move(callback), realmId = realmId.isolatedCopy(), expression = expression.isolatedCopy()](Inspector::CommandResult<String>&& result) mutable {
            CheckedPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            protectedThis->finishEvaluateBidiScriptResult(realmId, expression, WTF::move(result), WTF::move(callback));
        });
}

void BidiScriptAgent::finishEvaluateBidiScriptResult(const String& realmId, const String& expression, Inspector::CommandResult<String>&& result, Inspector::CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&& callback)
{
    // FIXME: Implement full BiDi exception details (stack traces, line/column numbers, exception type)
    // https://bugs.webkit.org/show_bug.cgi?id=304548
    using namespace Inspector::Protocol;

    if (!result.has_value()) {
        // IPC or internal error - NOT a JavaScript exception.
        // Include diagnostic information from the underlying error.
        String errorText = makeString("Internal error: script evaluation failed"_s);
        if (!result.error().isEmpty())
            errorText = makeString("Internal error: "_s, result.error());

        // Construct error object structure for RemoteValue.value per BiDi spec.
        auto errorObject = JSON::Object::create();
        errorObject->setString("message"_s, errorText);
        auto exceptionRemote = BidiScript::RemoteValue::create()
            .setType(BidiScript::RemoteValueType::Error)
            .release();
        exceptionRemote->setValue(WTF::move(errorObject));
        // stackTrace is required by protocol, but empty for internal failures (no JS stack available).
        auto stackTrace = BidiScript::StackTrace::create()
            .setCallFrames(JSON::ArrayOf<BidiScript::StackFrame>::create())
            .release();
        auto exceptionDetails = BidiScript::ExceptionDetails::create()
            .setText(errorText)
            .setLineNumber(0)
            .setColumnNumber(0)
            .setException(WTF::move(exceptionRemote))
            .setStackTrace(WTF::move(stackTrace))
            .release();
        callback({ { BidiScript::EvaluateResultType::Exception, realmId, nullptr, WTF::move(exceptionDetails) } });
        return;
    }

    auto envelopePayload = JSON::Value::parseJSON(result.value());
    auto envelopeObject = envelopePayload ? envelopePayload->asObject() : nullptr;

    // Handle malformed envelope (internal error - failed to parse or not an object).
    if (!envelopeObject) {
        String errorText = makeString("Internal error: malformed envelope in call to script.evaluate"_s);
        // Construct error object structure for RemoteValue.value per BiDi spec.
        auto errorObject = JSON::Object::create();
        errorObject->setString("message"_s, errorText);
        auto exceptionAsRemoteValue = BidiScript::RemoteValue::create()
            .setType(BidiScript::RemoteValueType::Error)
            .release();
        exceptionAsRemoteValue->setValue(WTF::move(errorObject));
        // stackTrace is required by protocol, but empty for internal failures (no JS stack available).
        auto stackTrace = BidiScript::StackTrace::create()
            .setCallFrames(JSON::ArrayOf<BidiScript::StackFrame>::create())
            .release();
        auto exceptionDetails = BidiScript::ExceptionDetails::create()
            .setText(errorText)
            .setLineNumber(0)
            .setColumnNumber(0)
            .setException(WTF::move(exceptionAsRemoteValue))
            .setStackTrace(WTF::move(stackTrace))
            .release();
        callback({ { BidiScript::EvaluateResultType::Exception, realmId, nullptr, WTF::move(exceptionDetails) } });
        return;
    }

    // Handle JavaScript exception (success == false means user's JS code threw).
    if (!envelopeObject->getBoolean("success"_s).value_or(false)) {
        String errorMessage = "JavaScript exception"_s;
        String errorName = "Error"_s;
        String errorStack;

        // Extract exception details from the envelope's "error" field.
        if (auto errorValue = envelopeObject->getValue("error"_s)) {
            if (auto errorObj = errorValue->asObject()) {
                if (auto message = errorObj->getString("message"_s); !message.isNull())
                    errorMessage = message;
                if (auto name = errorObj->getString("name"_s); !name.isNull())
                    errorName = name;
                if (auto stack = errorObj->getString("stack"_s); !stack.isNull())
                    errorStack = stack;
            }
        }

        // Construct a basic error object structure for RemoteValue.value.
        auto errorObject = JSON::Object::create();
        errorObject->setString("name"_s, errorName);
        errorObject->setString("message"_s, errorMessage);
        if (!errorStack.isNull())
            errorObject->setString("stack"_s, errorStack);

        auto exceptionRemote = BidiScript::RemoteValue::create()
            .setType(BidiScript::RemoteValueType::Error)
            .release();
        exceptionRemote->setValue(WTF::move(errorObject));

        // stackTrace is required by protocol - empty until full stack trace extraction is implemented (bug 304548).
        auto stackTrace = BidiScript::StackTrace::create()
            .setCallFrames(JSON::ArrayOf<BidiScript::StackFrame>::create())
            .release();
        auto exceptionDetails = BidiScript::ExceptionDetails::create()
            .setText(errorMessage)
            .setLineNumber(0)
            .setColumnNumber(0)
            .setException(WTF::move(exceptionRemote))
            .setStackTrace(WTF::move(stackTrace))
            .release();
        callback({ { BidiScript::EvaluateResultType::Exception, realmId, nullptr, WTF::move(exceptionDetails) } });
        return;
    }

    auto resultValue = envelopeObject->getValue("result"_s);
    auto remote = deserializeRemoteValue(resultValue.get());
    callback({ { BidiScript::EvaluateResultType::Success, realmId, WTF::move(remote), nullptr } });
}

// RealmRegistryStub implementation.
String BidiScriptAgent::RealmRegistryStub::realmIdForContext(const String& contextId) const
{
    return makeString("realm-"_s, contextId);
}

std::optional<String> BidiScriptAgent::RealmRegistryStub::contextForRealmId(const String& realmId) const
{
    if (realmId.startsWith("realm-"_s))
        return realmId.substring(6);
    return std::nullopt;
}

void BidiScriptAgent::getRealms(const BrowsingContext& optionalBrowsingContext, std::optional<Inspector::Protocol::BidiScript::RealmType>&& optionalRealmType, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::BidiScript::RealmInfo>>>&& callback)
{
    // https://w3c.github.io/webdriver-bidi/#command-script-getRealms

    // FIXME: Implement worker realm support (dedicated-worker, shared-worker, service-worker, worker).
    // https://bugs.webkit.org/show_bug.cgi?id=304300
    // Currently only window realms (main frames and iframes) are supported.
    // Worker realm types require tracking worker global scopes and their owner sets.

    // FIXME: Implement worklet realm support (paint-worklet, audio-worklet, worklet).
    // https://bugs.webkit.org/show_bug.cgi?id=304301

    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // Validate browsingContext parameter if provided and resolve its owning page.
    // Per W3C BiDi spec: the optional 'context' parameter is a browsingContext.BrowsingContext
    // that filters realms to those associated with the specified navigable (top-level or nested/iframe).
    std::optional<String> contextHandleFilter;
    RefPtr<WebPageProxy> resolvedPageForContext;
    if (!optionalBrowsingContext.isEmpty()) {
        contextHandleFilter = optionalBrowsingContext;

        // Only support page contexts in this PR - iframe support will be added later.
        if (optionalBrowsingContext.startsWith("page-"_s))
            resolvedPageForContext = session->webPageProxyForHandle(optionalBrowsingContext);
        else
            ASYNC_FAIL_WITH_PREDEFINED_ERROR(FrameNotFound);

        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!resolvedPageForContext, WindowNotFound);
    }

    // Early short-circuit: if a non-window realm type is requested, return empty (we currently only support window realms).
    if (optionalRealmType && *optionalRealmType != Inspector::Protocol::BidiScript::RealmType::Window) {
        auto realmsArray = JSON::ArrayOf<Inspector::Protocol::BidiScript::RealmInfo>::create();
        callback(WTF::move(realmsArray));
        return;
    }

    // Collect pages to process based on context filter.
    Deque<Ref<WebPageProxy>> pagesToProcess;

    if (contextHandleFilter && resolvedPageForContext)
        pagesToProcess.append(*resolvedPageForContext);
    else {
        // Enumerate all controlled pages; filtering by context happens during collection.
        for (Ref process : protect(session->processPool())->processes()) {
            for (Ref page : process->pages()) {
                if (page->isControlledByAutomation())
                    pagesToProcess.append(page);
            }
        }
    }

    if (pagesToProcess.isEmpty()) {
        auto realmsArray = JSON::ArrayOf<Inspector::Protocol::BidiScript::RealmInfo>::create();
        callback(WTF::move(realmsArray));
        return;
    }

    // Process pages asynchronously using getAllFrameTrees.
    processRealmsForPagesAsync(WTF::move(pagesToProcess), WTF::move(optionalRealmType), WTF::move(contextHandleFilter), { }, WTF::move(callback));
}


RefPtr<Inspector::Protocol::BidiScript::RealmInfo> BidiScriptAgent::createRealmInfoForFrame(const FrameInfoData& frameInfo)
{
    ASSERT(frameInfo.documentID);
    RefPtr session = m_session.get();
    if (!session)
        return nullptr;

    // Per W3C BiDi spec: "If you can't resolve the navigable (detached doc, bfcache edge),
    // return null for that settings — do not synthesize partial objects."
    auto contextHandle = contextHandleForFrame(frameInfo);
    if (!contextHandle)
        return nullptr;

    // Generate or reuse a realm id based on the frame's execution state so it changes on navigation/reload.
    String realmId = generateRealmIdForFrame(frameInfo);
    String origin = originStringFromSecurityOriginData(frameInfo.securityOrigin);

    auto realmInfo = Inspector::Protocol::BidiScript::RealmInfo::create()
        .setRealm(realmId)
        .setOrigin(origin)
        .setType(Inspector::Protocol::BidiScript::RealmType::Window)
        .release();

    // Set optional context field (required for window realms).
    realmInfo->setContext(*contextHandle);

    return realmInfo;
}

String BidiScriptAgent::generateRealmIdForFrame(const FrameInfoData& frameInfo)
{
    String currentURL = frameInfo.request.url().string();
    std::optional<String> currentDocumentID = frameInfo.documentID ? std::optional<String>(frameInfo.documentID->toString()) : std::nullopt;

    if (auto it = m_frameRealmCache.find(frameInfo.frameID); it != m_frameRealmCache.end()) {
        const auto& cachedEntry = it->value;

        if (cachedEntry.url == currentURL && cachedEntry.documentID == currentDocumentID)
            return cachedEntry.realmId;

        // FIXME: This is a workaround until realm.created/realm.destroyed events are implemented.
        // https://bugs.webkit.org/show_bug.cgi?id=304062
        // If only the documentID changed but URL is the same, reuse the cached realm ID to keep
        // realm IDs stable between getRealms() and evaluate()/callFunction() calls on the same document.
        // Once realm lifecycle events are implemented, they will handle cache updates properly.
        if (cachedEntry.url == currentURL && currentURL != "about:blank"_s) {
            m_frameRealmCache.set(frameInfo.frameID, FrameRealmCacheEntry { currentURL, currentDocumentID, cachedEntry.realmId });
            return cachedEntry.realmId;
        }

        // Special case: Transitioning to/from about:blank is typically not a navigation,
        // it's either the initial page load or a new test/session starting.
        // Don't treat this as a state change that increments the counter.
        bool transitioningToOrFromBlank = (cachedEntry.url == "about:blank"_s) != (currentURL == "about:blank"_s);

        if (transitioningToOrFromBlank) {
            m_frameRealmCache.remove(frameInfo.frameID);
            m_frameRealmCounters.remove(frameInfo.frameID);
        }
    }

    // Generate a new realm ID - the state has changed or this is a new frame.
    auto contextHandle = contextHandleForFrame(frameInfo);

    String newRealmId;

    if (!contextHandle) {
        // Fallback to frame-based ID if we can't get context handle.
        newRealmId = makeString("realm-frame-"_s, String::number(frameInfo.frameID.toUInt64()));
    } else {
        // Use the contextHandle directly - it's already unique for both main frames and iframes.
        // For the first load of a context, use just the context handle.
        // For subsequent navigations/reloads, append a counter to make it unique.
        auto counterIt = m_frameRealmCounters.find(frameInfo.frameID);
        if (counterIt == m_frameRealmCounters.end()) {
            // First realm for this frame - no counter suffix.
            newRealmId = makeString("realm-"_s, *contextHandle);
            // Start counter at 1 so the NEXT navigation will use "-1" suffix.
            m_frameRealmCounters.set(frameInfo.frameID, 1);
        } else {
            // Subsequent realm (reload/navigation) - use and increment counter.
            uint64_t counter = counterIt->value;
            newRealmId = makeString("realm-"_s, *contextHandle, "-"_s, String::number(counter));
            counterIt->value = counter + 1;
        }
    }

    // Update the cache with the new realm ID.
    m_frameRealmCache.set(frameInfo.frameID, FrameRealmCacheEntry { currentURL, currentDocumentID, newRealmId });

    return newRealmId;
}

String BidiScriptAgent::generateRealmIdForBrowsingContext(const String& browsingContext)
{
    // For evaluate/callFunction, we need to generate consistent realm IDs based on the browsing context.
    // This simplified version works for main window contexts (page handles).
    // For now, we just use the browsing context handle as the realm ID base.
    // This will match what getRealms() generates for main frames since contextHandleForFrame returns the page handle.

    // The realm ID should match the format used by generateRealmIdForFrame().
    return makeString("realm-"_s, browsingContext);
}

String BidiScriptAgent::originStringFromSecurityOriginData(const WebCore::SecurityOriginData& originData)
{
    if (originData.isOpaque())
        return "null"_s;

    return originData.toString();
}

void BidiScriptAgent::processRealmsForPagesAsync(Deque<Ref<WebPageProxy>>&& pagesToProcess, std::optional<Inspector::Protocol::BidiScript::RealmType>&& optionalRealmType, std::optional<String>&& contextHandleFilter, Vector<RefPtr<Inspector::Protocol::BidiScript::RealmInfo>>&& accumulated, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::BidiScript::RealmInfo>>>&& callback)
{
    if (pagesToProcess.isEmpty()) {
        // Assemble final array with window realms only.
        auto realmsArray = JSON::ArrayOf<Inspector::Protocol::BidiScript::RealmInfo>::create();
        for (auto& realmInfo : accumulated) {
            if (!realmInfo)
                continue;
            if (optionalRealmType && *optionalRealmType != Inspector::Protocol::BidiScript::RealmType::Window)
                continue; // Only window realms supported currently.

            realmsArray->addItem(realmInfo.releaseNonNull());
        }

        callback(WTF::move(realmsArray));
        return;
    }

    // Process the first page and recursively handle the rest.
    Ref<WebPageProxy> currentPage = pagesToProcess.first();
    pagesToProcess.removeFirst();

    currentPage->getAllFrameTrees([weakThis = WeakPtr { *this }, pagesToProcess = WTF::move(pagesToProcess), optionalRealmType = WTF::move(optionalRealmType), contextHandleFilter = WTF::move(contextHandleFilter), accumulated = WTF::move(accumulated), callback = WTF::move(callback)](Vector<FrameTreeNodeData>&& frameTrees) mutable {
        CheckedPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        // Collect realms from main frames only (no iframes in this PR).
        Vector<RefPtr<Inspector::Protocol::BidiScript::RealmInfo>> candidateRealms;
        for (const auto& frameTree : frameTrees)
            protectedThis->collectExecutionReadyFrameRealms(frameTree, candidateRealms, contextHandleFilter, false);

        for (auto& realmInfo : candidateRealms)
            accumulated.append(WTF::move(realmInfo));

        protectedThis->processRealmsForPagesAsync(WTF::move(pagesToProcess), WTF::move(optionalRealmType), WTF::move(contextHandleFilter), WTF::move(accumulated), WTF::move(callback));
    });
}

bool BidiScriptAgent::isFrameExecutionReady(const FrameInfoData& frameInfo)
{
    // Per W3C BiDi spec step 1: environment settings with execution ready flag set.
    // For enumerating realms (getRealms), we only require a committed document (documentID).
    // Remote frames (out-of-process) must still be considered: they have realms even if we
    // cannot execute scripts directly from the UI process.

    // Must have a valid document/script execution context.
    if (!frameInfo.documentID)
        return false;

    // Do not exclude remote frames for enumeration purposes.

    // We intentionally do not check errorOccurred. Per spec, iframe realms may exist despite
    // loading errors, and tests expect realms to be present.

    return true;
}

std::optional<String> BidiScriptAgent::contextHandleForFrame(const FrameInfoData& frameInfo)
{
    RefPtr session = m_session.get();
    if (!session)
        return std::nullopt;

    // FIXME: Add support for iframe contexts.
    // https://bugs.webkit.org/show_bug.cgi?id=304305
    if (!frameInfo.isMainFrame)
        return std::nullopt;

    if (frameInfo.webPageProxyID) {
        for (Ref process : protect(session->processPool())->processes()) {
            for (Ref page : process->pages()) {
                if (page->identifier() == *frameInfo.webPageProxyID)
                    return session->handleForWebPageProxy(page);
            }
        }
    }

    return std::nullopt;
}

void BidiScriptAgent::collectExecutionReadyFrameRealms(const FrameTreeNodeData& frameTree, Vector<RefPtr<Inspector::Protocol::BidiScript::RealmInfo>>& realms, const std::optional<String>& contextHandleFilter, bool recurseSubframes)
{
    // FIXME: Per W3C BiDi spec, when contextHandleFilter is present, we should also include
    // worker realms whose owner set includes the active document of that context.
    // Currently only collecting window realms (frames).

    // Check if frame is execution ready per W3C BiDi spec step 1:
    // "Let environment settings be a list of all the environment settings objects that have their execution ready flag set."
    if (isFrameExecutionReady(frameTree.info)) {
        auto handle = contextHandleForFrame(frameTree.info);
        bool shouldInclude = !contextHandleFilter || (handle && *handle == *contextHandleFilter);
        if (shouldInclude) {
            if (auto realmInfo = createRealmInfoForFrame(frameTree.info))
                realms.append(realmInfo);
        }
    }

    // FIXME: The recurseSubframes parameter is currently always called with false since this PR
    // only supports main frame contexts. When iframe support is added, this will be used to
    // recursively collect realms from nested browsing contexts (iframes).
    // Recurse into subframes if requested
    if (recurseSubframes) {
        for (const auto& child : frameTree.children)
            collectExecutionReadyFrameRealms(child, realms, contextHandleFilter, true);
    }
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
