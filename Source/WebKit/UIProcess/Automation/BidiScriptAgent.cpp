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
#include "BidiEventNames.h"
#include "FrameTreeNodeData.h"
#include "PageLoadState.h"
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProcessor.h"
#include "WebDriverBidiProtocolObjects.h"
#include "WebFrameMetrics.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <JavaScriptCore/MathCommon.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <algorithm>
#include <wtf/Borrow.h>
#include <wtf/Box.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/HexNumber.h>
#include <wtf/ProcessID.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebKit {

using namespace Inspector;
using BrowsingContext = Inspector::Protocol::BidiBrowsingContext::BrowsingContext;
using EvaluateResultType = Inspector::Protocol::BidiScript::EvaluateResultType;

static RefPtr<Inspector::Protocol::BidiScript::RemoteValue> deserializeRemoteValue(const JSON::Value*);
// parseLocalValue validates the complete LocalValue tree and produces a deserialized value when
// deserializedValue is non-null. The validation-only path avoids constructing a discarded JSON tree.
static bool parseLocalValue(const JSON::Value&, RefPtr<JSON::Value>* deserializedValue, bool* usesUnsupportedType, bool* usesHandle);
static RefPtr<JSON::Value> deserializeLocalValue(const JSON::Value&, bool* usesUnsupportedType = nullptr);
static bool validateLocalValue(const JSON::Value&, bool* usesUnsupportedType = nullptr, bool* usesHandle = nullptr);

static Inspector::CommandResult<void> updateValidatedDepthParameter(const JSON::Object& options, ASCIILiteral key, std::optional<double>& depth)
{
    if (auto json = options.getValue(key)) {
        if (json->isNull()) {
            depth = std::nullopt;
            return { };
        }

        auto value = WebDriverBidiProcessor::integerValue(*json, 0, JSC::maxSafeInteger());
        if (!value)
            return makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(InvalidParameter));
        depth = *value;
    }

    return { };
}

static String protocolRealmIdentifierString(RealmIdentifier realmIdentifier)
{
    return makeString("realm-"_s, realmIdentifier.loggingString());
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiScriptAgent);

BidiScriptAgent::BidiScriptAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_scriptDomainDispatcher(BidiScriptBackendDispatcher::create(backendDispatcher, this))
{
}

BidiScriptAgent::~BidiScriptAgent() = default;

// The WebProcess identifies a window by the identifier of its frame, which only the UI process can
// map to a browsing context id. This visits every place where a remote value can hold another, so a
// WebProcess can't put a window where a client would read it without the window being checked.
// FIXME: A window whose frame is gone gets no value, so clients that read value.context can fail. The
// spec's format requires a context id for every window, but its steps don't say what it is for such a window.
static void resolveWindowContexts(JSON::Value& value, WebAutomationSession& session)
{
    // A list holds remote values, and a mapping holds [key, remote value] pairs whose key can
    // also be a remote value.
    if (auto array = value.asArray()) {
        for (auto& item : *array)
            resolveWindowContexts(item, session);
        return;
    }

    auto remoteValue = value.asObject();
    if (!remoteValue)
        return;

    auto type = remoteValue->getString("type"_s);
    auto contents = remoteValue->getValue("value"_s);
    if (!contents)
        return;

    // A node's value holds remote values only as its children and shadow root. Its attributes aren't
    // remote values, even when they are named type and value.
    if (type == "node"_s) {
        if (auto properties = contents->asObject()) {
            if (auto children = properties->getValue("children"_s))
                resolveWindowContexts(*children, session);
            if (auto shadowRoot = properties->getValue("shadowRoot"_s))
                resolveWindowContexts(*shadowRoot, session);
        }
        return;
    }

    if (type != "window"_s) {
        resolveWindowContexts(*contents, session);
        return;
    }

    auto properties = contents->asObject();
    auto rawFrameID = properties ? parseInteger<uint64_t>(properties->getString("context"_s)) : std::nullopt;
    // FIXME: A frame removed after the window was serialized, but whose WebFrameProxy is still alive, gets a
    // new handle that no event announced and that is never removed.
    RefPtr frame = rawFrameID && WebCore::FrameIdentifier::isValidIdentifier(*rawFrameID) ? WebFrameProxy::webFrame(WebCore::FrameIdentifier(*rawFrameID)) : nullptr;
    RefPtr page = frame ? frame->page() : nullptr;
    // Unlike frameControlledBySession(), this accepts a frame in any process, because a window can be in a
    // process other than the one that serialized it.
    if (!page || page->activeAutomationSession().get() != &session) {
        remoteValue->remove("value"_s);
        return;
    }

    properties->setString("context"_s, session.effectiveHandleForWebFrameProxy(*frame));
}

static RefPtr<Inspector::Protocol::BidiScript::RemoteValue> deserializeRemoteValue(const JSON::Value* jsonValue)
{
    using RemoteValue = Inspector::Protocol::BidiScript::RemoteValue;
    using RemoteValueType = Inspector::Protocol::BidiScript::RemoteValueType;

    auto resultValue = RemoteValue::create();
    auto object = jsonValue ? jsonValue->asObject() : nullptr;
    if (!object)
        return resultValue.setType(RemoteValueType::Undefined).release();

    String typeString = object->getString("type"_s);
    auto parsedType = Inspector::Protocol::WebDriverBidiHelpers::parseEnumValueFromString<RemoteValueType>(typeString);
    if (!parsedType)
        return resultValue.setType(RemoteValueType::Undefined).release();

    auto remoteValue = resultValue.setType(*parsedType).release();
    if (auto value = object->getValue("value"_s))
        remoteValue->setValue(value.releaseNonNull());
    if (auto sharedId = object->getString("sharedId"_s); !sharedId.isNull())
        remoteValue->setSharedId(sharedId);
    if (auto handle = object->getString("handle"_s); !handle.isNull())
        remoteValue->setHandle(handle);
    if (auto internalId = object->getString("internalId"_s); !internalId.isNull())
        remoteValue->setInternalId(internalId);

    return remoteValue;
}

static bool parseLocalValue(const JSON::Value& jsonValue, RefPtr<JSON::Value>* deserializedValue, bool* usesUnsupportedType, bool* usesHandle)
{
    // FIXME: Implement RemoteReference and Channel types instead of using null placeholders.
    if (deserializedValue)
        *deserializedValue = nullptr;

    auto object = jsonValue.asObject();
    if (!object)
        return false;

    // RemoteReference alternatives take precedence over the other union members.
    if (object->getValue("sharedId"_s) || object->getValue("handle"_s)) {
        auto remoteReference = WebDriverBidiProcessor::parseRemoteReference(*object);
        if (!remoteReference)
            return false;
        if (usesUnsupportedType)
            *usesUnsupportedType = true;
        if (usesHandle && remoteReference->sharedId.isNull())
            *usesHandle = true;
        if (deserializedValue)
            *deserializedValue = JSON::Value::null();
        return true;
    }

    String typeString;
    if (!object->getString("type"_s, typeString))
        return false;

    if (typeString == "channel"_s) {
        auto value = object->getObject("value"_s);
        if (!value)
            return false;

        String channel;
        if (!value->getString("channel"_s, channel))
            return false;

        if (auto ownershipField = value->getValue("ownership"_s)) {
            String ownership;
            if (!ownershipField->asString(ownership) || (ownership != "root"_s && ownership != "none"_s))
                return false;
        }

        if (auto serializationOptionsField = value->getValue("serializationOptions"_s)) {
            auto serializationOptions = serializationOptionsField->asObject();
            if (!serializationOptions || !WebDriverBidiProcessor::validateSerializationOptions(*serializationOptions))
                return false;
        }

        if (usesUnsupportedType)
            *usesUnsupportedType = true;
        if (deserializedValue)
            *deserializedValue = JSON::Value::null();
        return true;
    }

    if (typeString == "string"_s || typeString == "bigint"_s || typeString == "date"_s) {
        auto value = object->getValue("value"_s);
        String stringValue;
        if (!value || !value->asString(stringValue))
            return false;
        if (deserializedValue)
            *deserializedValue = JSON::Value::create(stringValue);
        return true;
    }

    if (typeString == "number"_s) {
        auto value = object->getValue("value"_s);
        if (!value)
            return false;

        if (auto number = value->asDouble()) {
            if (deserializedValue)
                *deserializedValue = JSON::Value::create(*number);
            return true;
        }

        String specialValue;
        if (!value->asString(specialValue) || (specialValue != "NaN"_s && specialValue != "Infinity"_s && specialValue != "-Infinity"_s && specialValue != "-0"_s))
            return false;
        if (deserializedValue)
            *deserializedValue = JSON::Value::create(specialValue);
        return true;
    }

    if (typeString == "boolean"_s) {
        auto value = object->getValue("value"_s);
        if (!value)
            return false;
        auto booleanValue = value->asBoolean();
        if (!booleanValue)
            return false;
        if (deserializedValue)
            *deserializedValue = JSON::Value::create(*booleanValue);
        return true;
    }

    if (typeString == "undefined"_s || typeString == "null"_s) {
        if (deserializedValue)
            *deserializedValue = JSON::Value::null();
        return true;
    }

    if (typeString == "regexp"_s) {
        auto value = object->getObject("value"_s);
        if (!value)
            return false;

        String pattern;
        if (!value->getString("pattern"_s, pattern))
            return false;

        if (auto flagsField = value->getValue("flags"_s)) {
            String flags;
            if (!flagsField->asString(flags))
                return false;
        }

        if (deserializedValue)
            *deserializedValue = value;
        return true;
    }

    if (typeString == "array"_s || typeString == "set"_s) {
        auto valueArray = object->getArray("value"_s);
        if (!valueArray)
            return false;

        RefPtr<JSON::Array> resultArray;
        if (deserializedValue)
            resultArray = JSON::Array::create();
        for (unsigned i = 0; i < valueArray->length(); ++i) {
            Ref element = valueArray->get(i);
            RefPtr<JSON::Value> deserializedElement;
            if (!parseLocalValue(element.get(), deserializedValue ? &deserializedElement : nullptr, usesUnsupportedType, usesHandle))
                return false;
            if (deserializedValue)
                resultArray->pushValue(deserializedElement.releaseNonNull());
        }
        if (deserializedValue)
            *deserializedValue = resultArray;
        return true;
    }

    if (typeString == "object"_s) {
        auto valueArray = object->getArray("value"_s);
        if (!valueArray)
            return false;

        RefPtr<JSON::Object> resultObject;
        if (deserializedValue)
            resultObject = JSON::Object::create();
        for (unsigned i = 0; i < valueArray->length(); ++i) {
            auto pairArray = valueArray->get(i)->asArray();
            if (!pairArray || pairArray->length() != 2)
                return false;

            Ref valueElement = pairArray->get(1);
            RefPtr<JSON::Value> deserializedEntryValue;
            if (!parseLocalValue(valueElement.get(), deserializedValue ? &deserializedEntryValue : nullptr, usesUnsupportedType, usesHandle))
                return false;

            String key;
            Ref keyValue = pairArray->get(0);
            if (!keyValue->asString(key)) {
                RefPtr<JSON::Value> deserializedKey;
                if (!parseLocalValue(keyValue.get(), deserializedValue ? &deserializedKey : nullptr, usesUnsupportedType, usesHandle))
                    return false;
                if (deserializedValue && !deserializedKey->asString(key))
                    continue;
            }

            if (deserializedValue)
                resultObject->setValue(key, deserializedEntryValue.releaseNonNull());
        }
        if (deserializedValue)
            *deserializedValue = resultObject;
        return true;
    }

    if (typeString == "map"_s) {
        auto valueArray = object->getArray("value"_s);
        if (!valueArray)
            return false;

        RefPtr<JSON::Array> resultArray;
        if (deserializedValue)
            resultArray = JSON::Array::create();
        for (unsigned i = 0; i < valueArray->length(); ++i) {
            auto pairArray = valueArray->get(i)->asArray();
            if (!pairArray || pairArray->length() != 2)
                return false;

            RefPtr<JSON::Array> entryArray;
            if (deserializedValue)
                entryArray = JSON::Array::create();
            Ref keyElement = pairArray->get(0);
            String key;
            if (keyElement->asString(key)) {
                if (deserializedValue)
                    entryArray->pushValue(JSON::Value::create(key));
            } else {
                RefPtr<JSON::Value> deserializedKey;
                if (!parseLocalValue(keyElement.get(), deserializedValue ? &deserializedKey : nullptr, usesUnsupportedType, usesHandle))
                    return false;
                if (deserializedValue)
                    entryArray->pushValue(deserializedKey.releaseNonNull());
            }

            Ref valueElement = pairArray->get(1);
            RefPtr<JSON::Value> deserializedEntryValue;
            if (!parseLocalValue(valueElement.get(), deserializedValue ? &deserializedEntryValue : nullptr, usesUnsupportedType, usesHandle))
                return false;
            if (deserializedValue) {
                entryArray->pushValue(deserializedEntryValue.releaseNonNull());
                resultArray->pushArray(entryArray.releaseNonNull());
            }
        }
        if (deserializedValue)
            *deserializedValue = resultArray;
        return true;
    }

    return false;
}

static bool validateLocalValue(const JSON::Value& jsonValue, bool* usesUnsupportedType, bool* usesHandle)
{
    return parseLocalValue(jsonValue, nullptr, usesUnsupportedType, usesHandle);
}

static RefPtr<JSON::Value> deserializeLocalValue(const JSON::Value& jsonValue, bool* usesUnsupportedType)
{
    RefPtr<JSON::Value> deserializedValue;
    if (!parseLocalValue(jsonValue, &deserializedValue, usesUnsupportedType, nullptr))
        return nullptr;
    ASSERT(deserializedValue);
    return deserializedValue;
}

void BidiScriptAgent::callFunction(const String& functionDeclaration, bool awaitPromise, Ref<JSON::Object>&& target, RefPtr<JSON::Array>&& arguments, std::optional<Inspector::Protocol::BidiScript::ResultOwnership>&& resultOwnership, RefPtr<JSON::Object>&& optionalSerializationOptions, RefPtr<JSON::Object>&& optionalThis, std::optional<bool>&& optionalUserActivation, CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // Validate the complete request before resolving its target so malformed parameters take
    // precedence over missing contexts or realms.
    BrowsingContext contextTarget;
    String realmTarget;
    String sandbox;

    if (auto contextJSON = target->getValue("context"_s)) {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!contextJSON->asString(contextTarget), InvalidParameter);

        if (auto sandboxValue = target->getValue("sandbox"_s))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!sandboxValue->asString(sandbox), InvalidParameter);
    } else if (auto realmJSON = target->getValue("realm"_s))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!realmJSON->asString(realmTarget), InvalidParameter);
    else
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(InvalidParameter);

    bool argumentsUseUnsupportedType = false;
    bool argumentsUseHandle = false;
    auto argumentsArray = JSON::Array::create();
    if (arguments) {
        for (size_t i = 0; i < arguments->length(); ++i) {
            Ref argument = arguments->get(i);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!validateLocalValue(argument.get(), &argumentsUseUnsupportedType, &argumentsUseHandle), InvalidParameter);
            argumentsArray->pushValue(WTF::move(argument));
        }
    }

    if (optionalThis)
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!validateLocalValue(*optionalThis), InvalidParameter);

    if (optionalSerializationOptions) {
        auto result = WebDriverBidiProcessor::validateSerializationOptions(*optionalSerializationOptions);
        if (!result)
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(InvalidParameter, result.error());
    }

    BrowsingContext browsingContext;
    std::optional<RealmIdentifier> targetRealmIdentifier;
    if (!contextTarget.isNull()) {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(contextTarget.isEmpty(), FrameNotFound);
        browsingContext = contextTarget;
    } else {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(realmTarget.isEmpty(), FrameNotFound);
        targetRealmIdentifier = parseRealmIdentifier(realmTarget);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!targetRealmIdentifier, FrameNotFound);
        auto realmBrowsingContext = browsingContextForRealm(*targetRealmIdentifier);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!realmBrowsingContext, FrameNotFound);
        browsingContext = *realmBrowsingContext;
    }

    auto pageAndFrameHandles = session->extractBrowsingContextHandles(browsingContext);
    ASYNC_FAIL_IF_UNEXPECTED_RESULT(pageAndFrameHandles);
    auto& [topLevelContextHandle, frameHandle] = pageAndFrameHandles.value();

    // SerializationOptions defaults: DOM depth 0, object depth unlimited, and no shadow-tree children.
    // validateSerializationOptions() has already checked the values.
    std::optional<double> maxObjectDepth;
    std::optional<double> maxDomDepth { 0 };
    String includeShadowTree = "none"_s;
    if (optionalSerializationOptions) {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!updateValidatedDepthParameter(*optionalSerializationOptions, "maxObjectDepth"_s, maxObjectDepth), InvalidParameter);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!updateValidatedDepthParameter(*optionalSerializationOptions, "maxDomDepth"_s, maxDomDepth), InvalidParameter);
        if (auto value = optionalSerializationOptions->getString("includeShadowTree"_s); !value.isNull())
            includeShadowTree = value;
    }

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!sandbox.isEmpty(), NotImplemented, "Sandbox targets are not supported yet."_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(argumentsUseHandle, NoSuchHandle);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(argumentsUseUnsupportedType, NotImplemented, "Remote references and channels are not supported yet."_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(optionalThis, NotImplemented, "Custom this values are not supported yet."_s);

    // FIXME: Handle the `resultOwnership` option.
    // https://bugs.webkit.org/show_bug.cgi?id=328254
    // FIXME: Handle the custom `this` option.
    // https://bugs.webkit.org/show_bug.cgi?id=328207

    session->evaluateBidiCallFunction(topLevelContextHandle, frameHandle, targetRealmIdentifier, functionDeclaration, WTF::move(argumentsArray), awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, optionalUserActivation.value_or(false), std::nullopt,
        [weakThis = WeakPtr { *this }, callback = WTF::move(callback)](Inspector::CommandResultOf<String, RealmIdentifier>&& result) mutable {
            CheckedPtr protectedThis = weakThis;
            if (!protectedThis) {
                callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(InternalError)));
                return;
            }
            if (!result) {
                callback(makeUnexpected(result.error()));
                return;
            }

            auto [serializedResult, executionRealmIdentifier] = WTF::move(result.value());
            protectedThis->finishEvaluateBidiScriptResult(protocolRealmIdentifierString(executionRealmIdentifier), WTF::move(serializedResult), WTF::move(callback));
        });
}

void BidiScriptAgent::disown(Ref<JSON::Array>&& handles, Ref<JSON::Object>&& target, CommandCallback<void>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    BrowsingContext browsingContext;
    String realmID;
    String sandbox;
    bool hasBrowsingContext = target->find("context"_s) != target->end();
    bool hasRealm = target->find("realm"_s) != target->end();
    bool hasSandbox = target->find("sandbox"_s) != target->end();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(hasBrowsingContext && !target->getString("context"_s, browsingContext), InvalidParameter);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(hasRealm && !target->getString("realm"_s, realmID), InvalidParameter);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(hasSandbox && !target->getString("sandbox"_s, sandbox), InvalidParameter);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!hasRealm && !hasBrowsingContext, InvalidParameter);

    // Validate realm identifier early, even when context is also provided.
    std::optional<RealmIdentifier> realmIdentifier;
    if (hasRealm) {
        realmIdentifier = parseRealmIdentifier(realmID);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!realmIdentifier, FrameNotFound);
    }

    // Resolve target to a valid browsing context.
    if (!hasBrowsingContext) {
        auto resolvedBrowsingContext = browsingContextForRealm(*realmIdentifier);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!resolvedBrowsingContext, FrameNotFound);
        browsingContext = *resolvedBrowsingContext;
    }

    auto pageAndFrameHandles = session->extractBrowsingContextHandles(browsingContext);
    ASYNC_FAIL_IF_UNEXPECTED_RESULT(pageAndFrameHandles);

    // Validate handles array elements.
    for (size_t i = 0; i < handles->length(); ++i) {
        String handleString;
        if (!handles->get(i)->asString(handleString))
            ASYNC_FAIL_WITH_PREDEFINED_ERROR(InvalidParameter);
    }

    // No handles exist yet, because resultOwnership="root" isn't implemented, and the spec
    // ignores unknown handles. So there is nothing to release.
    // FIXME: Release the handles in the WebProcess once they exist. <https://webkit.org/b/328254>
    callback({ });
}

void BidiScriptAgent::evaluate(const String& expression, bool awaitPromise, Ref<JSON::Object>&& target, std::optional<Inspector::Protocol::BidiScript::ResultOwnership>&&, RefPtr<JSON::Object>&& optionalSerializationOptions, std::optional<bool>&&, CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // FIXME: Add full parameter validation (resultOwnership, sandbox, realm targets, etc.)
    // https://bugs.webkit.org/show_bug.cgi?id=288060
    std::optional<BrowsingContext> browsingContext = target->getString("context"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!browsingContext, InvalidParameter);

    auto pageAndFrameHandles = session->extractBrowsingContextHandles(*browsingContext);
    ASYNC_FAIL_IF_UNEXPECTED_RESULT(pageAndFrameHandles);
    auto& [topLevelContextHandle, frameHandle] = pageAndFrameHandles.value();

    // SerializationOptions defaults: DOM depth 0, object depth unlimited, and
    // no shadow-tree children.
    std::optional<double> maxObjectDepth;
    std::optional<double> maxDomDepth { 0 };
    String includeShadowTree = "none"_s;
    if (optionalSerializationOptions) {
        auto& serializationOptions = *optionalSerializationOptions;

        auto maxDomDepthValidationResult = updateValidatedDepthParameter(serializationOptions, "maxDomDepth"_s, maxDomDepth);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!maxDomDepthValidationResult, InvalidParameter);
        auto maxObjectDepthValidationResult = updateValidatedDepthParameter(serializationOptions, "maxObjectDepth"_s, maxObjectDepth);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!maxObjectDepthValidationResult, InvalidParameter);

        if (auto includeShadowTreeJSON = serializationOptions.getValue("includeShadowTree"_s)) {
            String includeShadowTreeValue;
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!includeShadowTreeJSON->asString(includeShadowTreeValue), InvalidParameter);
            auto parsedIncludeShadowTree = Inspector::Protocol::WebDriverBidiHelpers::parseEnumValueFromString<Inspector::Protocol::BidiScript::SerializationOptions::IncludeShadowTree>(includeShadowTreeValue);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!parsedIncludeShadowTree, InvalidParameter);
            includeShadowTree = includeShadowTreeValue;
        }
    }

    session->evaluateBidiScript(topLevelContextHandle, frameHandle, expression, awaitPromise, maxObjectDepth, maxDomDepth, includeShadowTree, std::nullopt,
        [weakThis = WeakPtr { *this }, callback = WTF::move(callback), browsingContext = browsingContext->isolatedCopy()](Inspector::CommandResult<String>&& result) mutable {
            CheckedPtr protectedThis = weakThis.get();
            if (!protectedThis) {
                callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(InternalError)));
                return;
            }
            String realmID = protectedThis->generateRealmIdForBrowsingContext(browsingContext);
            protectedThis->finishEvaluateBidiScriptResult(realmID, WTF::move(result), WTF::move(callback));
        });
}

void BidiScriptAgent::finishEvaluateBidiScriptResult(const String& realmID, Inspector::CommandResult<String>&& result, Inspector::CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&& callback)
{
    using namespace Inspector::Protocol;

    RefPtr session = m_session;
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // JavaScript exceptions arrive on the value channel below; CommandResult failures remain BiDi command errors.
    if (!result) {
        if (result.error().isEmpty()) {
            callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(InternalError)));
            return;
        }
        callback(makeUnexpected(result.error()));
        return;
    }

    auto parsedResult = JSON::Value::parseJSON(result.value());
    auto resultObject = parsedResult ? parsedResult->asObject() : nullptr;
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!resultObject, InternalError, "Malformed script result"_s);

    auto success = resultObject->getBoolean("success"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!success, InternalError, "Malformed script result"_s);

    if (!*success) {
        auto exceptionObject = resultObject->getObject("error"_s);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!exceptionObject, InternalError, "Malformed script result"_s);
        if (auto exception = exceptionObject->getValue("exception"_s))
            resolveWindowContexts(*exception, *session);
        auto exceptionDetails = buildExceptionDetailsFromExceptionObject(*exceptionObject);
        callback({ { BidiScript::EvaluateResultType::Exception, realmID, nullptr, WTF::move(exceptionDetails) } });
        return;
    }

    auto resultValue = resultObject->getValue("result"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!resultValue || !resultValue->asObject(), InternalError, "Malformed script result"_s);
    resolveWindowContexts(*resultValue, *session);
    auto remote = deserializeRemoteValue(resultValue.get());
    callback({ { BidiScript::EvaluateResultType::Success, realmID, WTF::move(remote), nullptr } });
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

        auto pageAndFrameHandles = session->extractBrowsingContextHandles(optionalBrowsingContext);
        ASYNC_FAIL_IF_UNEXPECTED_RESULT(pageAndFrameHandles);
        resolvedPageForContext = session->webPageProxyForHandle(pageAndFrameHandles.value().first);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!resolvedPageForContext, FrameNotFound);
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
        RefPtr processPool = session->processPool();
        for (Ref process : borrow(processPool->processes())) {
            for (Ref page : process->mainPages()) {
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

    // Process pages asynchronously using getAllFrames.
    processRealmsForPagesAsync(WTF::move(pagesToProcess), WTF::move(optionalRealmType), WTF::move(contextHandleFilter), { }, WTF::move(callback));
}

void BidiScriptAgent::addPreloadScript(const String& functionDeclaration, RefPtr<JSON::Array>&& optionalArguments, RefPtr<JSON::Array>&& optionalContexts, const String& optionalSandbox, RefPtr<JSON::Array>&& optionalUserContexts, Inspector::CommandCallback<String>&& callback)
{
    // FIXME: Add resource limits to prevent denial of service <https://webkit.org/b/288057>
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(functionDeclaration.isEmpty(), InvalidParameter, "functionDeclaration cannot be empty"_s);

    auto scriptID = PreloadScriptIdentifier::generate();

    // Validate mutual exclusion of contexts and userContexts
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(optionalContexts && optionalUserContexts, InvalidParameter, "contexts and userContexts are mutually exclusive"_s);

    Variant<AllContextsTag, Vector<String>> contexts { AllContextsTag { } };
    if (optionalContexts) {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!optionalContexts->length(), InvalidParameter, "contexts array cannot be empty"_s);

        Vector<String> contextList;
        for (auto& value : *optionalContexts) {
            String context;
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!value->asString(context), InvalidParameter, "contexts array must contain only strings"_s);

            // Look up the context first, then check if it's a top-level context per spec.
            RefPtr session = m_session.get();
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

            RefPtr page = session->webPageProxyForHandle(context);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!page, FrameNotFound);

            // Check if it's a top-level context (page handle format starts with "page-")
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!context.startsWith("page-"_s), InvalidParameter, "contexts must be top-level browsing contexts"_s);

            contextList.append(context);
        }
        contexts = WTF::move(contextList);
    }

    std::optional<Vector<String>> userContexts;
    if (optionalUserContexts) {
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!optionalUserContexts->length(), InvalidParameter, "userContexts array cannot be empty"_s);

        Vector<String> userContextList;
        for (auto& value : *optionalUserContexts) {
            String userContext;
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS_IF(!value->asString(userContext), InvalidParameter, "userContexts array must contain only strings"_s);

            // Validate userContext ID actually exists
            RefPtr session = m_session.get();
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session->isValidUserContext(userContext), NoSuchUserContext);

            userContextList.append(userContext);
        }
        userContexts = WTF::move(userContextList);
    }

    m_preloadScripts.append(std::make_pair(scriptID, PreloadScriptInfo {
        functionDeclaration,
        WTF::move(optionalArguments),
        WTF::move(contexts),
        optionalSandbox,
        WTF::move(userContexts)
    }));

    callback(makeString("preload-"_s, scriptID.toUInt64()));
}

void BidiScriptAgent::removePreloadScript(const String& script, Inspector::CommandCallback<void>&& callback)
{
    // Parse the script ID from the string (format: "preload-{number}")
    if (!script.startsWith("preload-"_s))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(NoSuchScript);

    auto idString = script.substring(8); // Skip "preload-" prefix
    auto idValue = parseInteger<uint64_t>(idString);
    if (!idValue || !PreloadScriptIdentifier::isValidIdentifier(*idValue))
        ASYNC_FAIL_WITH_PREDEFINED_ERROR(NoSuchScript);

    auto scriptID = PreloadScriptIdentifier(*idValue);
    bool found = m_preloadScripts.removeFirstMatching([&scriptID](const auto& pair) {
        return pair.first == scriptID;
    });

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!found, NoSuchScript);

    callback({ });
}

void BidiScriptAgent::executePreloadScriptsForContext(const String& browsingContext, const String& frameHandle)
{
    RefPtr session = m_session.get();
    if (!session)
        return;

    for (const auto& [scriptID, scriptInfo] : borrow(m_preloadScripts)) {
        // Check if this script applies to the current browsing context
        bool appliesToContext = WTF::switchOn(scriptInfo.contexts,
            [&] (const AllContextsTag&) {
                return true;
            },
            [&] (const Vector<String>& contextList) {
                return contextList.contains(browsingContext);
            });
        if (!appliesToContext)
            continue;

        // Check if this script applies to the current user context
        if (scriptInfo.userContexts) {
            RefPtr page = session->webPageProxyForHandle(browsingContext);
            if (!page)
                continue; // Skip if page no longer exists

            // Get the user context ID for this browsing context
            String pageUserContextID;
            if (page->sessionID() == PAL::SessionID::defaultSessionID())
                pageUserContextID = "default"_s;
            else
                pageUserContextID = makeString(hex(page->sessionID().toUInt64(), 16));

            // Check if the script's userContexts list includes this context
            if (!scriptInfo.userContexts->contains(pageUserContextID))
                continue;
        }

        // FIXME: Execute preload scripts in the sandbox realm specified by addPreloadScript. <https://webkit.org/b/305819>
        // FIXME: Create channels and remote references for preload script arguments in the target realm. <https://webkit.org/b/288057>

        // Deserialize LocalValue arguments into plain JSON values for script evaluation.
        auto argumentsArray = JSON::Array::create();
        if (scriptInfo.arguments) {
            for (unsigned i = 0; i < scriptInfo.arguments->length(); ++i) {
                Ref argValue = scriptInfo.arguments->get(i);
                if (auto parsedArgument = deserializeLocalValue(argValue.get()))
                    argumentsArray->pushValue(parsedArgument.releaseNonNull());
                else
                    argumentsArray->pushValue(JSON::Value::null());
            }
        }

        session->evaluateJavaScriptFunctionForBidi(
            browsingContext,
            frameHandle,
            scriptInfo.functionDeclaration,
            WTF::move(argumentsArray),
            false,
            [](auto) { }
        );
    }
}


Ref<Inspector::Protocol::BidiScript::RealmInfo> BidiScriptAgent::createProtocolRealmInfo(RealmIdentifier realmIdentifier, const RealmInfo& activeRealm)
{
    auto realmInfo = Inspector::Protocol::BidiScript::RealmInfo::create()
        .setRealm(protocolRealmIdentifierString(realmIdentifier))
        .setOrigin(originStringFromSecurityOriginData(activeRealm.origin))
        .setType(activeRealm.type)
        .release();

    if (activeRealm.context)
        realmInfo->setContext(*activeRealm.context);

    if (!activeRealm.owners.isEmpty()) {
        auto owners = JSON::ArrayOf<String>::create();
        for (auto owner : activeRealm.owners)
            owners->addItem(protocolRealmIdentifierString(owner));
        realmInfo->setOwners(WTF::move(owners));
    }

    return realmInfo;
}

String BidiScriptAgent::generateRealmIdForBrowsingContext(const String& browsingContext)
{
    // Look up the actual RealmIdentifier for this browsing context.
    auto it = m_browsingContextToRealmId.find(browsingContext);
    if (it != m_browsingContextToRealmId.end())
        return protocolRealmIdentifierString(it->value);

    // Fallback: if no realm exists yet, this is an error condition.
    // evaluate/callFunction should only be called on existing realms.
    return makeString("realm-"_s, browsingContext);
}

String BidiScriptAgent::originStringFromSecurityOriginData(const WebCore::SecurityOriginData& originData)
{
    if (originData.isOpaque())
        return "null"_s;

    return originData.toString();
}

std::optional<RealmIdentifier> BidiScriptAgent::registerDedicatedWorkerRealm(const WorkerIdentifier& workerIdentifier, WebCore::FrameIdentifier ownerFrameIdentifier, RealmIdentifier realmIdentifier, RealmIdentifier ownerRealmIdentifier, const BrowsingContext& ownerBrowsingContext, const WebCore::SecurityOriginData& origin, bool emitCreatedEvent)
{
    auto currentOwnerRealmIterator = m_browsingContextToRealmId.find(ownerBrowsingContext);
    if (currentOwnerRealmIterator == m_browsingContextToRealmId.end() || currentOwnerRealmIterator->value != ownerRealmIdentifier)
        return std::nullopt;

    auto ownerRealmIterator = m_activeRealms.find(ownerRealmIdentifier);
    if (ownerRealmIterator == m_activeRealms.end()
        || ownerRealmIterator->value.type != Inspector::Protocol::BidiScript::RealmType::Window
        || ownerRealmIterator->value.context != ownerBrowsingContext
        || ownerRealmIterator->value.frameIdentifier != ownerFrameIdentifier)
        return std::nullopt;

    auto workerRealmIterator = m_dedicatedWorkerRealms.find(realmIdentifier);
    if (workerRealmIterator != m_dedicatedWorkerRealms.end()) {
        if (workerRealmIterator->value.workerIdentifier != workerIdentifier
            || workerRealmIterator->value.ownerFrameIdentifier != ownerFrameIdentifier
            || workerRealmIterator->value.ownerRealmIdentifier != ownerRealmIdentifier
            || workerRealmIterator->value.ownerBrowsingContext != ownerBrowsingContext)
            return std::nullopt;
    } else
        m_dedicatedWorkerRealms.set(realmIdentifier, DedicatedWorkerRealmInfo { workerIdentifier.isolatedCopy(), ownerFrameIdentifier, ownerRealmIdentifier, ownerBrowsingContext.isolatedCopy() });

    auto activeRealmIterator = m_activeRealms.find(realmIdentifier);
    if (activeRealmIterator != m_activeRealms.end()) {
        if (emitCreatedEvent && !activeRealmIterator->value.creationNotified) {
            activeRealmIterator->value.creationNotified = true;
            sendRealmCreatedEvent(realmIdentifier, activeRealmIterator->value);
        }
        return realmIdentifier;
    }

    RealmInfo realmInfo {
        origin.isolatedCopy(),
        Inspector::Protocol::BidiScript::RealmType::DedicatedWorker,
        std::nullopt,
        { },
        { },
        emitCreatedEvent,
        std::nullopt
    };
    realmInfo.owners.append(ownerRealmIdentifier);
    realmInfo.associatedTopLevelContexts = ownerRealmIterator->value.associatedTopLevelContexts;
    m_activeRealms.set(realmIdentifier, WTF::move(realmInfo));

    if (emitCreatedEvent) {
        activeRealmIterator = m_activeRealms.find(realmIdentifier);
        ASSERT(activeRealmIterator != m_activeRealms.end());
        sendRealmCreatedEvent(realmIdentifier, activeRealmIterator->value);
    }

    return realmIdentifier;
}

void BidiScriptAgent::processRealmsForPagesAsync(Deque<Ref<WebPageProxy>>&& pagesToProcess, std::optional<Inspector::Protocol::BidiScript::RealmType>&& optionalRealmType, std::optional<String>&& contextHandleFilter, Vector<RefPtr<Inspector::Protocol::BidiScript::RealmInfo>>&& accumulated, Inspector::CommandCallback<Ref<JSON::ArrayOf<Inspector::Protocol::BidiScript::RealmInfo>>>&& callback)
{
    if (pagesToProcess.isEmpty()) {
        // Assemble final array with window realms only.
        auto realmsArray = JSON::ArrayOf<Inspector::Protocol::BidiScript::RealmInfo>::create();
        for (auto& realmInfo : borrow(accumulated)) {
            if (!realmInfo)
                continue;
            if (optionalRealmType && *optionalRealmType != Inspector::Protocol::BidiScript::RealmType::Window)
                continue; // Only window realms supported currently.

            realmsArray->addItem(realmInfo.releaseNonNull());
        }

        callback(WTF::move(realmsArray));
        return;
    }

    Ref<WebPageProxy> currentPage = pagesToProcess.first();
    pagesToProcess.removeFirst();

    currentPage->getAllFrames([weakThis = WeakPtr { *this }, pagesToProcess = WTF::move(pagesToProcess), optionalRealmType = WTF::move(optionalRealmType), contextHandleFilter = WTF::move(contextHandleFilter), accumulated = WTF::move(accumulated), callback = WTF::move(callback)](std::optional<FrameTreeNodeData>&& frameTree) mutable {
        CheckedPtr protectedThis = weakThis;
        if (!protectedThis) {
            callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(InternalError)));
            return;
        }
        Vector<FrameInfoData> candidateFrames;
        if (frameTree)
            protectedThis->collectExecutionReadyFrames(WTF::move(*frameTree), candidateFrames, contextHandleFilter);

        RefPtr session = protectedThis->m_session;
        if (!session) {
            callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(InternalError)));
            return;
        }

        auto candidateRealms = Box<Vector<RefPtr<Inspector::Protocol::BidiScript::RealmInfo>>>::create();
        candidateRealms->grow(candidateFrames.size());
        Ref aggregator = CallbackAggregator::create([weakThis, pagesToProcess = WTF::move(pagesToProcess), optionalRealmType = WTF::move(optionalRealmType), contextHandleFilter = WTF::move(contextHandleFilter), accumulated = WTF::move(accumulated), callback = WTF::move(callback), candidateRealms] mutable {
            CheckedPtr protectedThis = weakThis;
            if (!protectedThis) {
                callback(makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(InternalError)));
                return;
            }

            for (auto& realmInfo : *candidateRealms)
                accumulated.append(WTF::move(realmInfo));

            protectedThis->processRealmsForPagesAsync(WTF::move(pagesToProcess), WTF::move(optionalRealmType), WTF::move(contextHandleFilter), WTF::move(accumulated), WTF::move(callback));
        });

        size_t index = 0;
        for (auto& frameInfo : borrow(candidateFrames)) {
            auto contextHandle = protectedThis->contextHandleForFrame(frameInfo);
            if (!contextHandle) {
                ++index;
                continue;
            }

            auto pageAndFrameHandles = session->extractBrowsingContextHandles(*contextHandle);
            if (!pageAndFrameHandles) {
                ++index;
                continue;
            }

            session->getBidiRealmInfo(pageAndFrameHandles.value().first, pageAndFrameHandles.value().second, [weakThis, aggregator, candidateRealms, index, frameIdentifier = frameInfo.frameID](std::optional<RealmIdentifier>&& realmIdentifier) mutable {
                CheckedPtr protectedThis = weakThis;
                if (!protectedThis || !realmIdentifier)
                    return;

                auto activeRealm = protectedThis->m_activeRealms.find(*realmIdentifier);
                if (activeRealm == protectedThis->m_activeRealms.end()
                    || activeRealm->value.type != Inspector::Protocol::BidiScript::RealmType::Window
                    || !activeRealm->value.context
                    || activeRealm->value.frameIdentifier != frameIdentifier)
                    return;

                (*candidateRealms)[index] = protectedThis->createProtocolRealmInfo(*realmIdentifier, activeRealm->value);
            });
            ++index;
        }
    });
}

bool BidiScriptAgent::isFrameExecutionReady(const FrameInfoData& frameInfo)
{
    // Per W3C BiDi spec step 1: environment settings with execution ready flag set.
    // For enumerating realms (getRealms), we only require a committed document (documentID).
    // Remote frames (out-of-process) must still be considered: they have realms even if we
    // cannot execute scripts directly from the UI process.
    if (!frameInfo.documentID)
        return false;

    // We intentionally do not check errorOccurred. Per spec, iframe realms may exist despite
    // loading errors, and tests expect realms to be present.

    return true;
}

std::optional<String> BidiScriptAgent::contextHandleForFrame(const FrameInfoData& frameInfo)
{
    RefPtr session = m_session;
    if (!session)
        return std::nullopt;

    RefPtr frame = WebFrameProxy::webFrame(frameInfo.frameID);
    if (!frame)
        return std::nullopt;

    RefPtr page = frame->page();
    if (!page || !page->isControlledByAutomation())
        return std::nullopt;

    return session->effectiveHandleForWebFrameProxy(*frame);
}

void BidiScriptAgent::collectExecutionReadyFrames(FrameTreeNodeData&& frameTree, Vector<FrameInfoData>& frames, const std::optional<String>& contextHandleFilter)
{
    // FIXME: Per W3C BiDi spec, when contextHandleFilter is present, we should also include
    // worker realms whose owner set includes the active document of that context.
    // Currently only collecting window realms (frames).

    // Check if frame is execution ready per W3C BiDi spec step 1:
    // "Let environment settings be a list of all the environment settings objects that have their execution ready flag set."
    if (isFrameExecutionReady(frameTree.info)) {
        auto handle = contextHandleForFrame(frameTree.info);
        bool shouldInclude = !contextHandleFilter || (handle && *handle == *contextHandleFilter);
        if (shouldInclude)
            frames.append(WTF::move(frameTree.info));
    }

    for (auto& child : borrow(frameTree.children))
        collectExecutionReadyFrames(WTF::move(child), frames, contextHandleFilter);
}

void BidiScriptAgent::sendRealmCreatedEvent(RealmIdentifier realmIdentifier, const RealmInfo& realmInfo)
{
    RefPtr session = m_session.get();
    if (!session)
        return;

    session->bidiProcessor().emitEventIfEnabled(BidiEventNames::Script::RealmCreated, realmInfo.associatedTopLevelContexts, [&] {
        RefPtr<JSON::ArrayOf<String>> ownerRealms;
        if (!realmInfo.owners.isEmpty()) {
            ownerRealms = JSON::ArrayOf<String>::create();
            for (auto ownerRealm : realmInfo.owners)
                ownerRealms->addItem(protocolRealmIdentifierString(ownerRealm));
        }

        session->bidiProcessor().scriptDomainNotifier().realmCreated(
            protocolRealmIdentifierString(realmIdentifier),
            originStringFromSecurityOriginData(realmInfo.origin),
            realmInfo.type,
            realmInfo.context.value_or(nullString()),
            WTF::move(ownerRealms)
        );
    });
}

void BidiScriptAgent::sendRealmDestroyedEvent(RealmIdentifier realmIdentifier, const RealmInfo& realmInfo)
{
    RefPtr session = m_session;
    if (!session)
        return;

    session->bidiProcessor().emitEventIfEnabled(BidiEventNames::Script::RealmDestroyed, realmInfo.associatedTopLevelContexts, [&] {
        session->bidiProcessor().scriptDomainNotifier().realmDestroyed(protocolRealmIdentifierString(realmIdentifier));
    });
}

void BidiScriptAgent::notifyRealmCreatedFromBrowsingContext(RealmIdentifier realmIdentifier, WebCore::FrameIdentifier frameIdentifier, BrowsingContext browsingContext, BrowsingContext topLevelContext, const WebCore::SecurityOriginData& origin)
{
    auto previousRealmIterator = m_browsingContextToRealmId.find(browsingContext);
    if (previousRealmIterator != m_browsingContextToRealmId.end() && previousRealmIterator->value != realmIdentifier) {
        auto previousRealmIdentifier = previousRealmIterator->value;
        notifyRealmDestroyedFromBrowsingContext(previousRealmIdentifier, browsingContext);
    }

    m_browsingContextToRealmId.set(browsingContext, realmIdentifier);

    RealmInfo realmInfo {
        origin.isolatedCopy(),
        Inspector::Protocol::BidiScript::RealmType::Window,
        browsingContext,
        { },
        { },
        true,
        frameIdentifier
    };
    realmInfo.associatedTopLevelContexts.add(topLevelContext);
    auto addResult = m_activeRealms.set(realmIdentifier, WTF::move(realmInfo));
    sendRealmCreatedEvent(realmIdentifier, addResult.iterator->value);
}

void BidiScriptAgent::notifyRealmDestroyedFromBrowsingContext(RealmIdentifier realmIdentifier, Inspector::Protocol::BidiBrowsingContext::BrowsingContext browsingContext)
{
    auto activeRealmIterator = m_activeRealms.find(realmIdentifier);
    if (activeRealmIterator == m_activeRealms.end())
        return;
    if (activeRealmIterator->value.type != Inspector::Protocol::BidiScript::RealmType::Window
        || activeRealmIterator->value.context != browsingContext)
        return;

    RealmInfo realmInfo = WTF::move(activeRealmIterator->value);
    removeDedicatedWorkerRealmsForOwnerRealm(realmIdentifier);
    m_activeRealms.remove(realmIdentifier);

    auto browsingContextIterator = m_browsingContextToRealmId.find(browsingContext);
    if (browsingContextIterator != m_browsingContextToRealmId.end() && browsingContextIterator->value == realmIdentifier)
        m_browsingContextToRealmId.remove(browsingContextIterator);

    sendRealmDestroyedEvent(realmIdentifier, realmInfo);
}

void BidiScriptAgent::notifyRealmCreatedFromWorker(const WorkerIdentifier& workerIdentifier, WebCore::FrameIdentifier ownerFrameIdentifier, RealmIdentifier realmIdentifier, RealmIdentifier ownerRealmIdentifier, BrowsingContext ownerBrowsingContext, const WebCore::SecurityOriginData& origin)
{
    registerDedicatedWorkerRealm(workerIdentifier, ownerFrameIdentifier, realmIdentifier, ownerRealmIdentifier, ownerBrowsingContext, origin, true);
}

void BidiScriptAgent::notifyRealmDestroyedFromWorker(const WorkerIdentifier& workerIdentifier, WebCore::FrameIdentifier ownerFrameIdentifier, RealmIdentifier realmIdentifier, RealmIdentifier ownerRealmIdentifier)
{
    auto matches = dedicatedWorkerRealmMatches(realmIdentifier, workerIdentifier, ownerFrameIdentifier, ownerRealmIdentifier);
    if (!matches || !*matches)
        return;

    removeDedicatedWorkerRealm(realmIdentifier);
}

std::optional<bool> BidiScriptAgent::dedicatedWorkerRealmMatches(RealmIdentifier realmIdentifier, const WorkerIdentifier& workerIdentifier, WebCore::FrameIdentifier ownerFrameIdentifier, RealmIdentifier ownerRealmIdentifier) const
{
    auto workerRealmIterator = m_dedicatedWorkerRealms.find(realmIdentifier);
    if (workerRealmIterator == m_dedicatedWorkerRealms.end())
        return std::nullopt;

    return workerRealmIterator->value.workerIdentifier == workerIdentifier
        && workerRealmIterator->value.ownerFrameIdentifier == ownerFrameIdentifier
        && workerRealmIterator->value.ownerRealmIdentifier == ownerRealmIdentifier;
}

void BidiScriptAgent::removeDedicatedWorkerRealm(RealmIdentifier realmIdentifier)
{
    auto workerRealmIterator = m_dedicatedWorkerRealms.find(realmIdentifier);
    if (workerRealmIterator == m_dedicatedWorkerRealms.end())
        return;

    m_dedicatedWorkerRealms.remove(workerRealmIterator);

    auto activeRealmIterator = m_activeRealms.find(realmIdentifier);
    if (activeRealmIterator == m_activeRealms.end())
        return;

    RealmInfo realmInfo = WTF::move(activeRealmIterator->value);
    m_activeRealms.remove(activeRealmIterator);
    sendRealmDestroyedEvent(realmIdentifier, realmInfo);
}

void BidiScriptAgent::removeDedicatedWorkerRealmsForOwnerRealm(RealmIdentifier ownerRealmIdentifier)
{
    Vector<RealmIdentifier> realmIdentifiers;
    for (const auto& entry : m_dedicatedWorkerRealms) {
        if (entry.value.ownerRealmIdentifier == ownerRealmIdentifier)
            realmIdentifiers.append(entry.key);
    }

    for (auto realmIdentifier : realmIdentifiers)
        removeDedicatedWorkerRealm(realmIdentifier);
}

void BidiScriptAgent::removeDedicatedWorkerRealmsForBrowsingContext(const BrowsingContext& browsingContext)
{
    Vector<RealmIdentifier> realmIdentifiers;
    for (const auto& entry : m_dedicatedWorkerRealms) {
        if (entry.value.ownerBrowsingContext == browsingContext)
            realmIdentifiers.append(entry.key);
    }

    for (auto realmIdentifier : realmIdentifiers)
        removeDedicatedWorkerRealm(realmIdentifier);
}

void BidiScriptAgent::notifyRealmsDestroyedForProcess(WebCore::ProcessIdentifier processIdentifier)
{
    Vector<RealmIdentifier> realmIdentifiers;
    for (const auto& realmIdentifier : m_activeRealms.keys()) {
        if (realmIdentifier.processIdentifier() == processIdentifier)
            realmIdentifiers.append(realmIdentifier);
    }

    for (auto realmIdentifier : realmIdentifiers) {
        auto activeRealmIterator = m_activeRealms.find(realmIdentifier);
        if (activeRealmIterator == m_activeRealms.end())
            continue;

        if (activeRealmIterator->value.type == Inspector::Protocol::BidiScript::RealmType::DedicatedWorker) {
            removeDedicatedWorkerRealm(realmIdentifier);
            continue;
        }

        if (activeRealmIterator->value.type == Inspector::Protocol::BidiScript::RealmType::Window && activeRealmIterator->value.context) {
            notifyRealmDestroyedFromBrowsingContext(realmIdentifier, *activeRealmIterator->value.context);
            continue;
        }

        RealmInfo realmInfo = WTF::move(activeRealmIterator->value);
        m_activeRealms.remove(activeRealmIterator);
        sendRealmDestroyedEvent(realmIdentifier, realmInfo);
    }
}

std::optional<RealmIdentifier> BidiScriptAgent::realmIdentifierForBrowsingContext(const String& browsingContext) const
{
    auto it = m_browsingContextToRealmId.find(browsingContext);
    if (it == m_browsingContextToRealmId.end())
        return std::nullopt;
    return it->value;
}

void BidiScriptAgent::emitEventsForActiveRealms(const HashSet<String>& contextFilter)
{
    // Per W3C BiDi spec: when subscribing to script.realmCreated with subscribe priority 2,
    // emit events for all currently active realms.
    for (auto& entry : m_activeRealms) {
        RealmIdentifier realmIdentifier = entry.key;
        RealmInfo& realmInfo = entry.value;

        if (!contextFilter.isEmpty()) {
            bool matchesContextFilter = false;
            for (const auto& browsingContext : realmInfo.associatedTopLevelContexts) {
                if (contextFilter.contains(browsingContext)) {
                    matchesContextFilter = true;
                    break;
                }
            }
            if (!matchesContextFilter)
                continue;
        }

        sendRealmCreatedEvent(realmIdentifier, realmInfo);
        realmInfo.creationNotified = true;
    }
}

std::optional<RealmIdentifier> BidiScriptAgent::parseRealmIdentifier(const String& realmID)
{
    if (!realmID.startsWith("realm-"_s))
        return std::nullopt;

    auto serializedIdentifier = realmID.substring(6);
    auto separatorPosition = serializedIdentifier.find('-');
    if (separatorPosition == notFound || !separatorPosition || separatorPosition == serializedIdentifier.length() - 1
        || serializedIdentifier.find('-', separatorPosition + 1) != notFound)
        return std::nullopt;

    auto processIdentifierValue = parseInteger<uint64_t>(serializedIdentifier.left(separatorPosition));
    auto objectIdentifierValue = parseInteger<uint64_t>(serializedIdentifier.substring(separatorPosition + 1));
    if (!processIdentifierValue || !WebCore::ProcessIdentifier::isValidIdentifier(*processIdentifierValue)
        || !objectIdentifierValue || !NonProcessQualifiedRealmIdentifier::isValidIdentifier(*objectIdentifierValue))
        return std::nullopt;

    return RealmIdentifier { NonProcessQualifiedRealmIdentifier { *objectIdentifierValue }, WebCore::ProcessIdentifier { *processIdentifierValue } };
}

std::optional<String> BidiScriptAgent::browsingContextForRealm(RealmIdentifier realmIdentifier) const
{
    auto it = m_activeRealms.find(realmIdentifier);
    if (it == m_activeRealms.end())
        return std::nullopt;

    return it->value.context;
}

BidiScriptAgent::ParsedStackTrace BidiScriptAgent::parseStackTrace(const String& stackTraceString)
{
    using namespace Inspector::Protocol;

    auto callFrames = JSON::ArrayOf<BidiScript::StackFrame>::create();
    ParsedStackTrace result { .callFrames = callFrames };
    bool hasTopFrame = false;

    struct ParsedLocation {
        String url;
        unsigned lineNumber;
        unsigned columnNumber;
    };

    auto parseLocation = [](const String& location) -> std::optional<ParsedLocation> {
        auto lastColon = location.reverseFind(':');
        if (lastColon == notFound || !lastColon)
            return std::nullopt;
        auto secondLastColon = location.reverseFind(':', lastColon - 1);
        if (secondLastColon == notFound)
            return std::nullopt;
        auto parsedLine = parseInteger<unsigned>(location.substring(secondLastColon + 1, lastColon - secondLastColon - 1));
        auto parsedCol = parseInteger<unsigned>(location.substring(lastColon + 1));
        if (!parsedLine || !parsedCol || !*parsedLine || !*parsedCol)
            return std::nullopt;
        return ParsedLocation { location.left(secondLastColon), *parsedLine - 1, *parsedCol - 1 };
    };

    auto lines = stackTraceString.split(u'\n');
    for (auto& line : borrow(lines).get()) {
        String trimmed = line.trim(deprecatedIsSpaceOrNewline);
        if (trimmed.isEmpty())
            continue;

        // JSC stack-frame format: "functionName@url:line:column"
        auto atSign = trimmed.find('@');
        if (atSign == notFound)
            continue;

        String functionName = trimmed.left(atSign).trim(deprecatedIsSpaceOrNewline);
        auto parsedLocation = parseLocation(trimmed.substring(atSign + 1));
        if (!parsedLocation)
            continue;

        auto stackFrame = BidiScript::StackFrame::create()
            .setLineNumber(parsedLocation->lineNumber)
            .setColumnNumber(parsedLocation->columnNumber)
            .setFunctionName(functionName.isEmpty() ? emptyString() : functionName)
            .setUrl(parsedLocation->url)
            .release();
        callFrames->addItem(WTF::move(stackFrame));

        if (!hasTopFrame) {
            result.topLineNumber = parsedLocation->lineNumber;
            result.topColumnNumber = parsedLocation->columnNumber;
            hasTopFrame = true;
        }
    }

    return result;
}

Ref<Inspector::Protocol::BidiScript::ExceptionDetails> BidiScriptAgent::buildExceptionDetailsFromExceptionObject(const JSON::Object& exceptionObject)
{
    using namespace Inspector::Protocol;

    String exceptionMessage = "JavaScript exception occurred"_s;
    ParsedStackTrace parsedStack { .callFrames = JSON::ArrayOf<BidiScript::StackFrame>::create() };

    RefPtr<BidiScript::RemoteValue> exceptionValue;

    String message = exceptionObject.getString("message"_s);
    if (!message.isNull())
        exceptionMessage = message;

    String stack = exceptionObject.getString("stack"_s);
    if (!stack.isNull())
        parsedStack = parseStackTrace(stack);

    if (auto serializedException = exceptionObject.getValue("exception"_s)) {
        if (auto remoteValue = deserializeRemoteValue(serializedException.get()))
            exceptionValue = WTF::move(remoteValue);
    }

    if (!exceptionValue) {
        auto errorObject = JSON::Object::create();
        String name = exceptionObject.getString("name"_s);
        errorObject->setString("name"_s, name.isNull() ? "Error"_s : name);
        errorObject->setString("message"_s, exceptionMessage);
        if (!stack.isNull())
            errorObject->setString("stack"_s, stack);

        exceptionValue = BidiScript::RemoteValue::create()
            .setType(BidiScript::RemoteValueType::Error)
            .release();
        exceptionValue->setValue(WTF::move(errorObject));
    }

    return BidiScript::ExceptionDetails::create()
        .setLineNumber(parsedStack.topLineNumber)
        .setColumnNumber(parsedStack.topColumnNumber)
        .setText(exceptionMessage)
        .setException(exceptionValue.releaseNonNull())
        .setStackTrace(BidiScript::StackTrace::create()
            .setCallFrames(WTF::move(parsedStack.callFrames))
            .release())
        .release();
}

Ref<Inspector::Protocol::BidiScript::StackTrace> BidiScriptAgent::createEmptyStackTrace()
{
    return Inspector::Protocol::BidiScript::StackTrace::create()
        .setCallFrames(JSON::ArrayOf<Inspector::Protocol::BidiScript::StackFrame>::create())
        .release();
}

Ref<Inspector::Protocol::BidiScript::ExceptionDetails> BidiScriptAgent::createExceptionDetails(const String& text, Ref<Inspector::Protocol::BidiScript::RemoteValue>&& exceptionValue, unsigned lineNumber, unsigned columnNumber)
{
    return Inspector::Protocol::BidiScript::ExceptionDetails::create()
        .setLineNumber(lineNumber)
        .setColumnNumber(columnNumber)
        .setText(text)
        .setException(WTF::move(exceptionValue))
        .setStackTrace(createEmptyStackTrace())
        .release();
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
