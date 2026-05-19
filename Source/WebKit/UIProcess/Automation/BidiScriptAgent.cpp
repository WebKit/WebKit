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
#include <WebCore/FrameIdentifier.h>
#include <WebCore/SecurityOrigin.h>
#include <WebCore/SecurityOriginData.h>
#include <algorithm>
#include <wtf/Borrow.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/ProcessID.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebKit {

using namespace Inspector;
using BrowsingContext = Inspector::Protocol::BidiBrowsingContext::BrowsingContext;
using EvaluateResultType = Inspector::Protocol::BidiScript::EvaluateResultType;

static RefPtr<Inspector::Protocol::BidiScript::RemoteValue> deserializeRemoteValue(const JSON::Value*);
static Ref<JSON::Value> deserializeLocalValue(const JSON::Value&);

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiScriptAgent);

BidiScriptAgent::BidiScriptAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_scriptDomainDispatcher(BidiScriptBackendDispatcher::create(backendDispatcher, this))
{
}

BidiScriptAgent::~BidiScriptAgent() = default;

static RefPtr<Inspector::Protocol::BidiScript::RemoteValue> deserializeRemoteValue(const JSON::Value* jsonValue)
{
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
    if (typeString == "undefined"_s)
        return resultValue.setType(RemoteValueType::Undefined).release();
    if (typeString == "symbol"_s)
        return resultValue.setType(RemoteValueType::Symbol).release();
    if (typeString == "function"_s)
        return resultValue.setType(RemoteValueType::Function).release();

    // Structured types without a serialized value.
    if (typeString == "promise"_s)
        return resultValue.setType(RemoteValueType::Promise).release();
    if (typeString == "error"_s)
        return resultValue.setType(RemoteValueType::Error).release();
    if (typeString == "weakmap"_s)
        return resultValue.setType(RemoteValueType::Weakmap).release();
    if (typeString == "weakset"_s)
        return resultValue.setType(RemoteValueType::Weakset).release();
    if (typeString == "arraybuffer"_s)
        return resultValue.setType(RemoteValueType::Arraybuffer).release();
    if (typeString == "typedarray"_s)
        return resultValue.setType(RemoteValueType::Typedarray).release();
    if (typeString == "generator"_s)
        return resultValue.setType(RemoteValueType::Generator).release();
    if (typeString == "proxy"_s)
        return resultValue.setType(RemoteValueType::Proxy).release();

    // Structured types with a serialized value.
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
    if (typeString == "object"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::Object).release();
        if (auto value = object->getValue("value"_s))
            remoteValue->setValue(value.releaseNonNull());
        return remoteValue;
    }
    if (typeString == "array"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::Array).release();
        if (auto value = object->getValue("value"_s))
            remoteValue->setValue(value.releaseNonNull());
        return remoteValue;
    }
    if (typeString == "map"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::Map).release();
        if (auto value = object->getValue("value"_s))
            remoteValue->setValue(value.releaseNonNull());
        return remoteValue;
    }
    if (typeString == "set"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::Set).release();
        if (auto value = object->getValue("value"_s))
            remoteValue->setValue(value.releaseNonNull());
        return remoteValue;
    }
    if (typeString == "node"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::Node).release();
        if (auto value = object->getValue("value"_s))
            remoteValue->setValue(value.releaseNonNull());
        return remoteValue;
    }
    if (typeString == "nodelist"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::Nodelist).release();
        if (auto value = object->getValue("value"_s))
            remoteValue->setValue(value.releaseNonNull());
        return remoteValue;
    }
    if (typeString == "htmlcollection"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::HTMLcollection).release();
        if (auto value = object->getValue("value"_s))
            remoteValue->setValue(value.releaseNonNull());
        return remoteValue;
    }
    if (typeString == "window"_s) {
        auto remoteValue = resultValue.setType(RemoteValueType::Window).release();
        if (auto value = object->getValue("value"_s))
            remoteValue->setValue(value.releaseNonNull());
        return remoteValue;
    }

    return resultValue.setType(RemoteValueType::Undefined).release();
}

// Converts a BiDi LocalValue JSON object into a JavaScript source expression string.
// Used to construct the argument list and this-value for script.callFunction.
static String localValueToJSExpression(const JSON::Value& localValue)
{
    auto object = localValue.asObject();
    if (!object)
        return "undefined"_s;

    // A remote reference has no "type" field (it uses sharedId / handle instead).
    // FIXME: implement remote reference lookup https://bugs.webkit.org/show_bug.cgi?id=288057
    String typeString = object->getString("type"_s);
    if (typeString.isNull())
        return "undefined"_s;

    if (typeString == "undefined"_s)
        return "undefined"_s;
    if (typeString == "null"_s)
        return "null"_s;

    if (typeString == "boolean"_s)
        return object->getBoolean("value"_s).value_or(false) ? "true"_s : "false"_s;

    if (typeString == "string"_s)
        return JSON::Value::create(object->getString("value"_s))->toJSONString();

    if (typeString == "number"_s) {
        if (auto num = object->getDouble("value"_s))
            return JSON::Value::create(*num)->toJSONString();
        String special = object->getString("value"_s);
        if (special == "NaN"_s)
            return "NaN"_s;
        if (special == "Infinity"_s)
            return "Infinity"_s;
        if (special == "-Infinity"_s)
            return "-Infinity"_s;
        if (special == "-0"_s)
            return "-0"_s;
        return "0"_s;
    }

    if (typeString == "bigint"_s)
        return makeString(object->getString("value"_s), 'n');

    if (typeString == "date"_s)
        return makeString("new Date("_s, JSON::Value::create(object->getString("value"_s))->toJSONString(), ")"_s);

    if (typeString == "regexp"_s) {
        RefPtr valueObj = object->getObject("value"_s);
        if (!valueObj)
            return "undefined"_s;
        String patternJSON = JSON::Value::create(valueObj->getString("pattern"_s))->toJSONString();
        String flags = valueObj->getString("flags"_s);
        if (flags.isEmpty())
            return makeString("new RegExp("_s, patternJSON, ")"_s);
        return makeString("new RegExp("_s, patternJSON, ","_s, JSON::Value::create(flags)->toJSONString(), ")"_s);
    }

    if (typeString == "array"_s) {
        RefPtr valueArray = object->getArray("value"_s);
        if (!valueArray || !valueArray->length())
            return "[]"_s;
        StringBuilder sb;
        sb.append('[');
        for (unsigned i = 0; i < valueArray->length(); ++i) {
            if (i > 0)
                sb.append(',');
            sb.append(localValueToJSExpression(valueArray->get(i)));
        }
        sb.append(']');
        return sb.toString();
    }

    if (typeString == "object"_s) {
        // MappingLocalValue = [*[(LocalValue / text), LocalValue]]
        RefPtr mappingArray = object->getArray("value"_s);
        if (!mappingArray || !mappingArray->length())
            return "({})"_s;
        StringBuilder sb;
        sb.append("({"_s);
        bool first = true;
        for (unsigned i = 0; i < mappingArray->length(); ++i) {
            RefPtr<JSON::Array> pair = mappingArray->get(i)->asArray();
            if (!pair || pair->length() < 2)
                continue;
            if (!first)
                sb.append(',');
            first = false;
            Ref keyValue = pair->get(0);
            // Key is either a plain JSON string or a LocalValue.
            String keyStr = keyValue->asString();
            if (!keyStr.isNull())
                sb.append(JSON::Value::create(keyStr)->toJSONString());
            else
                sb.append(makeString('[', localValueToJSExpression(keyValue.get()), ']'));
            sb.append(':');
            sb.append(localValueToJSExpression(pair->get(1)));
        }
        sb.append("})"_s);
        return sb.toString();
    }

    if (typeString == "map"_s) {
        RefPtr mappingArray = object->getArray("value"_s);
        if (!mappingArray || !mappingArray->length())
            return "new Map()"_s;
        StringBuilder sb;
        sb.append("new Map(["_s);
        for (unsigned i = 0; i < mappingArray->length(); ++i) {
            if (i > 0)
                sb.append(',');
            RefPtr<JSON::Array> pair = mappingArray->get(i)->asArray();
            if (!pair || pair->length() < 2) {
                sb.append("[undefined,undefined]"_s);
                continue;
            }
            sb.append('[');
            Ref keyValue = pair->get(0);
            String keyStr = keyValue->asString();
            if (!keyStr.isNull())
                sb.append(JSON::Value::create(keyStr)->toJSONString());
            else
                sb.append(localValueToJSExpression(keyValue.get()));
            sb.append(',');
            sb.append(localValueToJSExpression(pair->get(1)));
            sb.append(']');
        }
        sb.append("])"_s);
        return sb.toString();
    }

    if (typeString == "set"_s) {
        RefPtr valueArray = object->getArray("value"_s);
        if (!valueArray || !valueArray->length())
            return "new Set()"_s;
        StringBuilder sb;
        sb.append("new Set(["_s);
        for (unsigned i = 0; i < valueArray->length(); ++i) {
            if (i > 0)
                sb.append(',');
            sb.append(localValueToJSExpression(valueArray->get(i)));
        }
        sb.append("])"_s);
        return sb.toString();
    }

    // channel type: not yet implemented.
    // FIXME: implement channel deserialization https://bugs.webkit.org/show_bug.cgi?id=288057
    return "undefined"_s;
}

// FIXME: Implement RemoteReference and Channel types https://bugs.webkit.org/show_bug.cgi?id=288057
static Ref<JSON::Value> deserializeLocalValue(const JSON::Value& jsonValue)
{
    auto object = jsonValue.asObject();
    if (!object)
        return const_cast<JSON::Value&>(jsonValue);

    String typeString = object->getString("type"_s);

    if (typeString == "string"_s)
        return JSON::Value::create(object->getString("value"_s));

    if (typeString == "number"_s) {
        if (auto num = object->getDouble("value"_s))
            return JSON::Value::create(*num);
        // FIXME: special values NaN, Infinity, -Infinity, -0 are lost here since JSON has no representation for them.
        return JSON::Value::create(object->getString("value"_s));
    }

    if (typeString == "boolean"_s) {
        if (auto b = object->getBoolean("value"_s))
            return JSON::Value::create(*b);
        return JSON::Value::create(false);
    }

    if (typeString == "bigint"_s)
        return JSON::Value::create(object->getString("value"_s));

    if (typeString == "undefined"_s || typeString == "null"_s)
        return JSON::Value::null();

    if (typeString == "date"_s)
        return JSON::Value::create(object->getString("value"_s));

    if (typeString == "regexp"_s) {
        if (auto value = object->getValue("value"_s))
            return value.releaseNonNull();
        return JSON::Value::null();
    }

    if (typeString == "array"_s) {
        auto valueArray = object->getArray("value"_s);
        if (!valueArray)
            return JSON::Array::create();

        auto resultArray = JSON::Array::create();
        for (unsigned i = 0; i < valueArray->length(); ++i) {
            Ref element = valueArray->get(i);
            resultArray->pushValue(deserializeLocalValue(element.get()));
        }
        return resultArray;
    }

    if (typeString == "object"_s) {
        auto valueArray = object->getArray("value"_s);
        if (!valueArray)
            return JSON::Object::create();

        auto resultObject = JSON::Object::create();
        for (unsigned i = 0; i < valueArray->length(); ++i) {
            auto pairArray = valueArray->get(i)->asArray();
            if (!pairArray || pairArray->length() < 2)
                continue;

            Ref keyValue = pairArray->get(0);
            String key;
            if (keyValue->asObject())
                key = deserializeLocalValue(keyValue.get())->asString();
            else
                key = keyValue->asString();

            if (!key.isEmpty()) {
                Ref valueElement = pairArray->get(1);
                resultObject->setValue(key, deserializeLocalValue(valueElement.get()));
            }
        }
        return resultObject;
    }

    if (typeString == "map"_s) {
        auto valueArray = object->getArray("value"_s);
        if (!valueArray)
            return JSON::Array::create();

        auto resultArray = JSON::Array::create();
        for (unsigned i = 0; i < valueArray->length(); ++i) {
            auto pairValue = valueArray->get(i);
            auto pairArray = pairValue->asArray();
            if (!pairArray || pairArray->length() < 2)
                continue;

            auto entryArray = JSON::Array::create();
            Ref keyElement = pairArray->get(0);
            entryArray->pushValue(deserializeLocalValue(keyElement.get()));
            Ref valueElement = pairArray->get(1);
            entryArray->pushValue(deserializeLocalValue(valueElement.get()));

            resultArray->pushArray(WTF::move(entryArray));
        }
        return resultArray;
    }

    if (typeString == "set"_s) {
        auto valueArray = object->getArray("value"_s);
        if (!valueArray)
            return JSON::Array::create();

        auto resultArray = JSON::Array::create();
        for (unsigned i = 0; i < valueArray->length(); ++i) {
            Ref element = valueArray->get(i);
            resultArray->pushValue(deserializeLocalValue(element.get()));
        }
        return resultArray;
    }

    return JSON::Value::null();
}

void BidiScriptAgent::callFunction(const String& functionDeclaration, bool awaitPromise, Ref<JSON::Object>&& target, RefPtr<JSON::Array>&& arguments, std::optional<Inspector::Protocol::BidiScript::ResultOwnership>&&, RefPtr<JSON::Object>&& optionalSerializationOptions, RefPtr<JSON::Object>&& optionalThis, std::optional<bool>&& optionalUserActivation, CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // FIXME: handle non-BrowsingContext obtained from `Target` (realm targets).
    std::optional<BrowsingContext> browsingContext = target->getString("context"_s);
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!browsingContext, InvalidParameter);

    auto pageAndFrameHandles = session->extractBrowsingContextHandles(*browsingContext);
    ASYNC_FAIL_IF_UNEXPECTED_RESULT(pageAndFrameHandles);
    auto& [topLevelContextHandle, frameHandle] = pageAndFrameHandles.value();

    // FIXME: handle `resultOwnership` option.
    // FIXME: handle `serializationOptions` option.
    // FIXME: handle `userActivation` option.
    // FIXME: implement channel-type argument deserialization https://bugs.webkit.org/show_bug.cgi?id=288057

    String thisExpression = optionalThis ? localValueToJSExpression(*optionalThis) : "undefined"_s;

    StringBuilder argsBuilder;
    if (arguments) {
        for (unsigned i = 0; i < arguments->length(); ++i) {
            if (i > 0)
                argsBuilder.append(',');
            argsBuilder.append(localValueToJSExpression(arguments->get(i)));
        }
    }

    // Wrap as (fn).call(this, args) so evaluateBidiScript serializes the result as a BiDi RemoteValue.
    // https://bugs.webkit.org/show_bug.cgi?id=288058
    String callExpression = argsBuilder.isEmpty()
        ? makeString("("_s, functionDeclaration, ").call("_s, thisExpression, ")"_s)
        : makeString("("_s, functionDeclaration, ").call("_s, thisExpression, ","_s, argsBuilder.toString(), ")"_s);

    String realmID = generateRealmIdForBrowsingContext(*browsingContext);

    session->evaluateBidiScript(topLevelContextHandle, frameHandle, callExpression, awaitPromise, 1, std::nullopt,
        [weakThis = WeakPtr { *this }, callback = WTF::move(callback), realmID = realmID.isolatedCopy(), callExpression = callExpression.isolatedCopy()](Inspector::CommandResult<String>&& result) mutable {
            CheckedPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            protectedThis->finishEvaluateBidiScriptResult(realmID, callExpression, WTF::move(result), WTF::move(callback));
        });
}

void BidiScriptAgent::disown(Ref<JSON::Array>&& handles, Ref<JSON::Object>&& target, CommandCallback<void>&& callback)
{
    // FIXME: WebProcess forwarding to actually release JavaScript objects will be added
    // once resultOwnership="root" support is implemented. https://bugs.webkit.org/show_bug.cgi?id=288059

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

    // FIXME: Implement WebProcess forwarding to release JavaScript objects.
    // https://bugs.webkit.org/show_bug.cgi?id=288059
    ASYNC_FAIL_WITH_PREDEFINED_ERROR(NotImplemented);
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

    String realmID = generateRealmIdForBrowsingContext(*browsingContext);

    session->evaluateBidiScript(*browsingContext, emptyString(), expression, awaitPromise, 1, std::nullopt,
        [weakThis = WeakPtr { *this }, callback = WTF::move(callback), realmID = realmID.isolatedCopy(), expression = expression.isolatedCopy()](Inspector::CommandResult<String>&& result) mutable {
            CheckedPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            protectedThis->finishEvaluateBidiScriptResult(realmID, expression, WTF::move(result), WTF::move(callback));
        });
}

void BidiScriptAgent::finishEvaluateBidiScriptResult(const String& realmID, const String& expression, Inspector::CommandResult<String>&& result, Inspector::CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&& callback)
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
        callback({ { BidiScript::EvaluateResultType::Exception, realmID, nullptr, WTF::move(exceptionDetails) } });
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
        callback({ { BidiScript::EvaluateResultType::Exception, realmID, nullptr, WTF::move(exceptionDetails) } });
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
        callback({ { BidiScript::EvaluateResultType::Exception, realmID, nullptr, WTF::move(exceptionDetails) } });
        return;
    }

    auto resultValue = envelopeObject->getValue("result"_s);
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
        RefPtr processPool = session->processPool();
        for (Ref process : borrow(processPool->processes()).get()) {
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

    for (const auto& [scriptID, scriptInfo] : m_preloadScripts) {
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
                argumentsArray->pushValue(deserializeLocalValue(argValue.get()));
            }
        }

        session->evaluateJavaScriptFunction(
            browsingContext,
            frameHandle,
            scriptInfo.functionDeclaration,
            WTF::move(argumentsArray),
            false,
            false,
            std::nullopt,
            [](auto) { }
        );
    }
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

    RealmIdentifier realmIdentifier = generateRealmIdForFrame(frameInfo);
    String realmID = makeString("realm-"_s, realmIdentifier.loggingString());
    String origin = originStringFromSecurityOriginData(frameInfo.securityOrigin);

    auto realmInfo = Inspector::Protocol::BidiScript::RealmInfo::create()
        .setRealm(realmID)
        .setOrigin(origin)
        .setType(Inspector::Protocol::BidiScript::RealmType::Window)
        .release();

    realmInfo->setContext(*contextHandle);

    return realmInfo;
}

RealmIdentifier BidiScriptAgent::generateRealmIdForFrame(const FrameInfoData& frameInfo)
{
    auto contextHandle = contextHandleForFrame(frameInfo);
    if (!contextHandle) {
        // Fallback: generate a new realm identifier if we can't get context handle.
        return RealmIdentifier::generate();
    }

    // Look up existing realm identifier for this browsing context.
    auto it = m_browsingContextToRealmId.find(*contextHandle);
    if (it == m_browsingContextToRealmId.end()) {
        // No realm has been created yet for this context - generate a new identifier.
        auto newRealmId = RealmIdentifier::generate();
        m_browsingContextToRealmId.set(*contextHandle, newRealmId);
        return newRealmId;
    }

    return it->value;
}

String BidiScriptAgent::generateRealmIdForBrowsingContext(const String& browsingContext)
{
    // Look up the actual RealmIdentifier for this browsing context.
    auto it = m_browsingContextToRealmId.find(browsingContext);
    if (it != m_browsingContextToRealmId.end())
        return makeString("realm-"_s, it->value.loggingString());

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
    if (!frameInfo.documentID)
        return false;

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
        RefPtr processPool = session->processPool();
        for (Ref process : borrow(processPool->processes()).get()) {
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
    if (recurseSubframes) {
        for (const auto& child : frameTree.children)
            collectExecutionReadyFrameRealms(child, realms, contextHandleFilter, true);
    }
}

void BidiScriptAgent::sendRealmCreatedEvent(const String& realmID, const WebCore::SecurityOriginData& origin, Inspector::Protocol::BidiScript::RealmType type, Inspector::Protocol::BidiBrowsingContext::BrowsingContext context)
{
    RefPtr session = m_session.get();
    if (!session)
        return;

    session->bidiProcessor().emitEventIfEnabled(BidiEventNames::Script::RealmCreated, { context }, [&]() {
        session->bidiProcessor().scriptDomainNotifier().realmCreated(realmID, originStringFromSecurityOriginData(origin), type, context);
    });
}

void BidiScriptAgent::notifyRealmCreated(RealmIdentifier realmIdentifier, Inspector::Protocol::BidiBrowsingContext::BrowsingContext browsingContext, const WebCore::SecurityOriginData& origin)
{
    // The WebProcess owns realm identifier creation and passes the identifier across IPC.
    String realmID = makeString("realm-"_s, realmIdentifier.loggingString());

    // Track the current realm for this browsing context.
    m_browsingContextToRealmId.set(browsingContext, realmIdentifier);

    RealmInfo realmInfo { origin.isolatedCopy(), Inspector::Protocol::BidiScript::RealmType::Window, browsingContext };
    m_activeRealms.set(realmIdentifier, WTF::move(realmInfo));

    sendRealmCreatedEvent(realmID, origin, Inspector::Protocol::BidiScript::RealmType::Window, browsingContext);
}

void BidiScriptAgent::notifyRealmDestroyed(RealmIdentifier realmIdentifier, Inspector::Protocol::BidiBrowsingContext::BrowsingContext browsingContext)
{
    RefPtr session = m_session.get();
    if (!session)
        return;

    // Match the realm identifier that the WebProcess reported for this realm.
    String realmID = makeString("realm-"_s, realmIdentifier.loggingString());

    // Remove the realm from active realms.
    m_activeRealms.remove(realmIdentifier);

    // Remove the browsing context mapping (realm will be regenerated on next navigation).
    m_browsingContextToRealmId.remove(browsingContext);

    session->bidiProcessor().emitEventIfEnabled(BidiEventNames::Script::RealmDestroyed, { browsingContext }, [&]() {
        session->bidiProcessor().scriptDomainNotifier().realmDestroyed(realmID, browsingContext);
    });
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
    for (const auto& entry : m_activeRealms) {
        const RealmIdentifier& realmIdentifier = entry.key;
        const RealmInfo& realmInfo = entry.value;

        if (!contextFilter.isEmpty() && !contextFilter.contains(realmInfo.context))
            continue;

        String realmID = makeString("realm-"_s, realmIdentifier.loggingString());
        sendRealmCreatedEvent(realmID, realmInfo.origin, realmInfo.type, realmInfo.context);
    }
}

std::optional<RealmIdentifier> BidiScriptAgent::parseRealmIdentifier(const String& realmID)
{
    if (!realmID.startsWith("realm-"_s))
        return std::nullopt;

    auto idValue = parseInteger<uint64_t>(realmID.substring(6));
    if (!idValue || !RealmIdentifier::isValidIdentifier(*idValue))
        return std::nullopt;

    return RealmIdentifier(*idValue);
}

std::optional<String> BidiScriptAgent::browsingContextForRealm(RealmIdentifier realmIdentifier) const
{
    auto it = m_activeRealms.find(realmIdentifier);
    if (it == m_activeRealms.end())
        return std::nullopt;

    return it->value.context;
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)
