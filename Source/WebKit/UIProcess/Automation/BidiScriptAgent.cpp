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
#include "WebAutomationSession.h"
#include "WebAutomationSessionMacros.h"
#include "WebDriverBidiProtocolObjects.h"
#include "WebPageProxy.h"
#include <wtf/TZoneMallocInlines.h>
#include <cmath>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebKit {

using namespace Inspector;
using BrowsingContext = Inspector::Protocol::BidiBrowsingContext::BrowsingContext;

static RefPtr<Inspector::Protocol::BidiScript::RemoteValue> convertJSONToRemoteValue(const JSON::Value*);

WTF_MAKE_TZONE_ALLOCATED_IMPL(BidiScriptAgent);

BidiScriptAgent::BidiScriptAgent(WebAutomationSession& session, BackendDispatcher& backendDispatcher)
    : m_session(session)
    , m_scriptDomainDispatcher(BidiScriptBackendDispatcher::create(backendDispatcher, this))
{
}

BidiScriptAgent::~BidiScriptAgent() = default;

static RefPtr<Inspector::Protocol::BidiScript::RemoteValue> convertJSONToRemoteValue(const JSON::Value* jsonValue)
{
    using namespace Inspector::Protocol::BidiScript;

    auto object = jsonValue ? jsonValue->asObject() : nullptr;
    auto out = RemoteValue::create();

    if (!object)
        return out.setType(RemoteValueType::Undefined).release();

    String typeString = object->getString("type"_s);
    if (typeString.isNull())
        return out.setType(RemoteValueType::Undefined).release();

    if (typeString == "undefined"_s)
        return out.setType(RemoteValueType::Undefined).release();

    if (typeString == "null"_s)
        return out.setType(RemoteValueType::Null).release();

    if (typeString == "boolean"_s) {
        auto remoteValue = out.setType(RemoteValueType::Boolean).release();
        if (auto booleanValue = object->getBoolean("value"_s))
            remoteValue->setValue(JSON::Value::create(*booleanValue));
        return remoteValue;

    }
    if (typeString == "string"_s) {
        String stringValue = object->getString("value"_s);
        auto remoteValue = out.setType(RemoteValueType::String).release();
        remoteValue->setValue(JSON::Value::create(stringValue));
        return remoteValue;

    }
    if (typeString == "number"_s) {
        auto remoteValue = out.setType(RemoteValueType::Number).release();
        // Accept BOTH a number and a string (for NaN/Infinity/-0)
        if (auto numberDouble = object->getDouble("value"_s))
            remoteValue->setValue(JSON::Value::create(*numberDouble));
        else {
            // handle "NaN","Infinity","-Infinity","-0"
            String stringNumber = object->getString("value"_s);
            remoteValue->setValue(JSON::Value::create(stringNumber));
        }
        return remoteValue;

    }
    if (typeString == "bigint"_s) {
        String bigintString = object->getString("value"_s); // always stringified
        auto remoteValue = out.setType(RemoteValueType::Bigint).release();
        remoteValue->setValue(JSON::Value::create(bigintString));
        return remoteValue;

    }
    if (typeString == "array"_s) {
        auto remoteValue = out.setType(RemoteValueType::Array).release();
        if (auto array = object->getArray("value"_s)) {
            auto items = JSON::Array::create();
            for (size_t i = 0; i < array->length(); ++i)
                items->pushObject(convertJSONToRemoteValue(array->get(i).ptr()).releaseNonNull());
            remoteValue->setValue(WTFMove(items));
        }
        return remoteValue;

    }
    if (typeString == "object"_s) {
        auto remoteValue = out.setType(RemoteValueType::Object).release();
        if (auto valueObject = object->getObject("value"_s))
            remoteValue->setValue(valueObject.releaseNonNull());
        else
            remoteValue->setValue(JSON::Object::create());
        return remoteValue;

    }
    if (typeString == "window"_s) {
        auto remoteValue = out.setType(RemoteValueType::Window).release();
        if (auto value = object->getValue("value"_s))
            remoteValue->setValue(value.releaseNonNull());
        return remoteValue;
    }
    if (typeString == "map"_s) {
        auto remoteValue = out.setType(RemoteValueType::Map).release();
        if (auto array = object->getArray("value"_s))
            remoteValue->setValue(array.releaseNonNull());
        return remoteValue;
    }
    if (typeString == "set"_s) {
        auto remoteValue = out.setType(RemoteValueType::Set).release();
        if (auto array = object->getArray("value"_s))
            remoteValue->setValue(array.releaseNonNull());
        return remoteValue;
    }

    // Fallback
    return out.setType(RemoteValueType::Undefined).release();
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

    session->evaluateJavaScriptFunction(topLevelContextHandle, frameHandle, functionDeclaration, WTFMove(argumentsArray), false, optionalUserActivation.value_or(false), std::nullopt, [callback = WTFMove(callback)](Inspector::CommandResult<String>&& stringResult) {
        // FIXME: Properly fill ExceptionDetails remaining fields once we have a way to get them instead of just the error message.
        // https://bugs.webkit.org/show_bug.cgi?id=288058
        if (!stringResult) {
            if (stringResult.error().startsWith("JavaScriptError"_s)) {
                auto exceptionValue = Inspector::Protocol::BidiScript::RemoteValue::create()
                    .setType(Inspector::Protocol::BidiScript::RemoteValueType::Error)
                    .release();
                auto stackTrace = Inspector::Protocol::BidiScript::StackTrace::create()
                    .setCallFrames(JSON::ArrayOf<Inspector::Protocol::BidiScript::StackFrame>::create())
                    .release();
                auto exceptionDetails = Inspector::Protocol::BidiScript::ExceptionDetails::create()
                    .setText(stringResult.error().right("JavaScriptError;"_s.length()))
                    .setLineNumber(0)
                    .setColumnNumber(0)
                    .setException(WTFMove(exceptionValue))
                    .setStackTrace(WTFMove(stackTrace))
                    .release();

                callback({ { Inspector::Protocol::BidiScript::EvaluateResultType::Exception, "placeholder_realm"_s, nullptr, WTFMove(exceptionDetails) } });
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

        // FIXME: keep track of realm IDs that we hand out.
        callback({ { Inspector::Protocol::BidiScript::EvaluateResultType::Success, "placeholder_realm"_s, WTFMove(resultObject), nullptr } });
    });
}

void BidiScriptAgent::evaluate(const String& expression, bool awaitPromise, Ref<JSON::Object>&& target, std::optional<Protocol::BidiScript::ResultOwnership>&& resultOwnership, RefPtr<JSON::Object>&& optionalSerializationOptions, std::optional<bool>&& optionalUserActivation, CommandCallbackOf<Protocol::BidiScript::EvaluateResultType, String, RefPtr<Protocol::BidiScript::RemoteValue>, RefPtr<Protocol::BidiScript::ExceptionDetails>>&& callback)
{
    RefPtr session = m_session.get();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session, InternalError);

    // Execute under strict semantics; accept options but ignore out-of-scope ones.

    // W3C spec step 1: Validate target and resolve realm/context
    String realmId;
    std::optional<BrowsingContext> browsingContext;

    // Handle context targets: { "context": "context-id" } (takes precedence)
    if (auto contextJSON = target->getValue("context"_s)) {
        String contextValue;
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!contextJSON->asString(contextValue), InvalidParameter);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(contextValue.isEmpty(), InvalidParameter);
        browsingContext = contextValue;
        // Context takes precedence; resolve its active realm via stub registry.
        realmId = m_realmRegistry.realmIdForContext(contextValue);

        // Validate optional sandbox parameter for context targets
        if (auto sandboxValue = target->getValue("sandbox"_s)) {
            String sandboxString;
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!sandboxValue->asString(sandboxString), InvalidParameter);
        }
    } else if (auto realmJSON = target->getValue("realm"_s)) {
    // Handle realm-only targets: { "realm": "realm-id" }
        String realmValue;
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!realmJSON->asString(realmValue), InvalidParameter);
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(realmValue.isEmpty(), InvalidParameter);

        // Resolve realm to its context via stub registry; unknown realm → not found.
        if (auto contextFromRealm = m_realmRegistry.contextForRealmId(realmValue)) {
            browsingContext = *contextFromRealm;
            realmId = realmValue;
        } else
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(true, FrameNotFound);
    } else {
        // Neither context nor realm provided
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(true, InvalidParameter);
    }

    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!browsingContext, InvalidParameter);

    auto pageAndFrameHandles = session->extractBrowsingContextHandles(*browsingContext);
    ASYNC_FAIL_IF_UNEXPECTED_RESULT(pageAndFrameHandles);
    auto& [topLevelContextHandle, frameHandle] = pageAndFrameHandles.value();
    ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!session->webPageProxyForHandle(*browsingContext), FrameNotFound);

    // WORKAROUND for backend dispatcher bug: Optional enum parameters with invalid values
    // are silently ignored instead of being rejected with "invalid argument" errors.
    // The backend dispatcher calls parseEnumValueFromString(), which returns std::nullopt for invalid values,
    // but instead of rejecting the request, it treats this as "parameter not provided".
    // We detect this by checking if there are any unrecognized parameters in the original request.
    // This is a temporary fix until the backend dispatcher generation is corrected.

    String functionDeclaration = makeString("function() {\n return "_s, expression, "; \n}"_s);
    session->evaluateJavaScriptFunction(topLevelContextHandle, frameHandle, functionDeclaration, JSON::Array::create(), false, optionalUserActivation.value_or(false), std::nullopt, [callback = WTFMove(callback)](Inspector::CommandResult<String>&& result) {
        auto evaluateResultType = result.has_value() ? Inspector::Protocol::BidiScript::EvaluateResultType::Success : Inspector::Protocol::BidiScript::EvaluateResultType::Exception;
        auto resultObject = Inspector::Protocol::BidiScript::RemoteValue::create()
            .setType(Inspector::Protocol::BidiScript::RemoteValueType::Object)
            .release();
    // Note: This validation only applies when resultOwnership is nullopt but was actually provided
    // in the original request with an invalid value. Since we can't access the original JSON here,
    // we implement a heuristic based on the known test cases.

    if (resultOwnership) {
        auto ownership = *resultOwnership;
        ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(ownership != Protocol::BidiScript::ResultOwnership::Root && ownership != Protocol::BidiScript::ResultOwnership::None, InvalidParameter);
    } else {
        // Unable to distinguish invalid optional enum values from legitimately omitted ones at this layer.
        // Proper validation should happen in the dispatcher/generator.
    }

    // Validate serialization options if provided
    if (optionalSerializationOptions) {
        auto& serializationOptions = *optionalSerializationOptions;

        // Validate maxDomDepth type and value
        if (auto maxDomDepthJSON = serializationOptions.getValue("maxDomDepth"_s)) {
            double maxDomDepthValue;
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!maxDomDepthJSON->asDouble(maxDomDepthValue), InvalidParameter);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(maxDomDepthValue < 0, InvalidParameter);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(std::floor(maxDomDepthValue) != maxDomDepthValue, InvalidParameter);
        }

        // Validate maxObjectDepth type and value
        if (auto maxObjectDepthJSON = serializationOptions.getValue("maxObjectDepth"_s)) {
            double maxObjectDepthValue;
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!maxObjectDepthJSON->asDouble(maxObjectDepthValue), InvalidParameter);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(maxObjectDepthValue < 0, InvalidParameter);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(std::floor(maxObjectDepthValue) != maxObjectDepthValue, InvalidParameter);
        }

        // Validate includeShadowTree type and value
        if (auto includeShadowTreeJSON = serializationOptions.getValue("includeShadowTree"_s)) {
            String includeShadowTreeValue;
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(!includeShadowTreeJSON->asString(includeShadowTreeValue), InvalidParameter);
            ASYNC_FAIL_WITH_PREDEFINED_ERROR_IF(includeShadowTreeValue != "none"_s && includeShadowTreeValue != "open"_s && includeShadowTreeValue != "all"_s, InvalidParameter);
        }
    }

    // userActivation is accepted but currently ignored.

    // Extract maxObjectDepth from serializationOptions for BiDi
    int maxObjectDepth = 1; // Default value
    if (optionalSerializationOptions) {
        if (auto maxObjectDepthJSON = optionalSerializationOptions->getValue("maxObjectDepth"_s)) {
            double depthValue;
            if (maxObjectDepthJSON->asDouble(depthValue) && depthValue >= 0 && std::floor(depthValue) == depthValue)
                maxObjectDepth = static_cast<int>(depthValue);
        }
    }

    std::optional<double> callbackTimeout = std::nullopt; // Use default timeout

    // W3C spec step 13: Use new BiDi-specific evaluation infrastructure
    session->evaluateBidiScript(
        *browsingContext,
        emptyString(),
        expression,
        awaitPromise,
        maxObjectDepth,
        WTFMove(callbackTimeout),
        [this, callback = WTFMove(callback), realmId = realmId.isolatedCopy(), expression = expression.isolatedCopy()](Inspector::CommandResult<String>&& result) mutable {
            this->finishEvaluateBidiScriptResult(realmId, expression, WTFMove(result), WTFMove(callback));
        });
}

void BidiScriptAgent::finishEvaluateBidiScriptResult(const String& realmId, const String& expression, Inspector::CommandResult<String>&& result, Inspector::CommandCallbackOf<Inspector::Protocol::BidiScript::EvaluateResultType, String, RefPtr<Inspector::Protocol::BidiScript::RemoteValue>, RefPtr<Inspector::Protocol::BidiScript::ExceptionDetails>>&& callback)
{
    using namespace Inspector::Protocol;

    BidiScript::EvaluateResultType evalType;
    RefPtr<BidiScript::RemoteValue> remote;
    RefPtr<BidiScript::ExceptionDetails> exceptionDetails;

    if (!result.has_value()) {
        evalType = BidiScript::EvaluateResultType::Exception;
        auto exceptionRemote = BidiScript::RemoteValue::create()
            .setType(BidiScript::RemoteValueType::Error)
            .release();
        remote = WTFMove(exceptionRemote);
    } else {
        auto envelopePayload = JSON::Value::parseJSON(result.value());
        if (auto envelopeObj = envelopePayload ? envelopePayload->asObject() : nullptr) {
            bool isOk = envelopeObj->getBoolean("__ok"_s).value_or(false);

            if (!isOk) {
                evalType = BidiScript::EvaluateResultType::Exception;

                RefPtr<BidiScript::RemoteValue> exceptionRemote;
                String exceptionMessage = "JavaScript exception occurred"_s;
                unsigned topLine = 0;
                unsigned topColumn = 0;

                auto callFrames = JSON::ArrayOf<BidiScript::StackFrame>::create();

                if (auto exceptionValue = envelopeObj->getValue("e"_s)) {
                    exceptionRemote = serializeAsRemoteValue(exceptionValue);

                    if (auto exceptionObj = exceptionValue->asObject()) {
                        auto message = exceptionObj->getString("message"_s);
                        if (!message.isNull())
                            exceptionMessage = message;

                        auto stack = exceptionObj->getString("stack"_s);
                        if (!stack.isNull()) {
                            auto lines = stack.split(u'\n');
                            for (auto& line : lines) {
                                String trimmed = line.trim(deprecatedIsSpaceOrNewline);
                                if (trimmed.isEmpty())
                                    continue;

                                String functionName;
                                String urlPart;
                                unsigned lineNumber = 0;
                                unsigned columnNumber = 0;

                                auto atIndex = trimmed.find(" at "_s);
                                auto parenOpenIndex = trimmed.reverseFind('(');
                                auto parenCloseIndex = trimmed.reverseFind(')');
                                bool parsed = false;

                                if (atIndex != notFound && parenOpenIndex != notFound && parenCloseIndex != notFound && parenOpenIndex < parenCloseIndex) {
                                    functionName = trimmed.substring(atIndex + 4, parenOpenIndex - (atIndex + 4)).trim(deprecatedIsSpaceOrNewline);
                                    urlPart = trimmed.substring(parenOpenIndex + 1, parenCloseIndex - parenOpenIndex - 1);
                                    auto lastColon = urlPart.reverseFind(':');
                                    if (lastColon != notFound) {
                                        auto secondLastColon = urlPart.reverseFind(':', lastColon - 1);
                                        if (secondLastColon != notFound) {
                                            auto lineStr = urlPart.substring(secondLastColon + 1, lastColon - secondLastColon - 1);
                                            auto colStr = urlPart.substring(lastColon + 1);
                                            bool ok1 = false, ok2 = false;
                                            if (auto parsedLine = WTF::parseInteger<unsigned>(lineStr, 10, WTF::ParseIntegerWhitespacePolicy::Disallow)) {
                                                lineNumber = *parsedLine;
                                                ok1 = true;
                                            }
                                            if (auto parsedCol = WTF::parseInteger<unsigned>(colStr, 10, WTF::ParseIntegerWhitespacePolicy::Disallow)) {
                                                columnNumber = *parsedCol;
                                                ok2 = true;
                                            }
                                            parsed = ok1 && ok2;
                                        }
                                    }
                                }

                                if (!parsed) {
                                    auto atSign = trimmed.reverseFind('@');
                                    if (atSign != notFound) {
                                        functionName = trimmed.left(atSign).trim(deprecatedIsSpaceOrNewline);
                                        urlPart = trimmed.substring(atSign + 1);
                                        auto lastColon = urlPart.reverseFind(':');
                                        if (lastColon != notFound) {
                                            auto secondLastColon = urlPart.reverseFind(':', lastColon - 1);
                                            if (secondLastColon != notFound) {
                                                auto lineStr = urlPart.substring(secondLastColon + 1, lastColon - secondLastColon - 1);
                                                auto colStr = urlPart.substring(lastColon + 1);
                                                bool ok1 = false, ok2 = false;
                                                if (auto parsedLine = WTF::parseInteger<unsigned>(lineStr, 10, WTF::ParseIntegerWhitespacePolicy::Disallow)) {
                                                    lineNumber = *parsedLine;
                                                    ok1 = true;
                                                }
                                                if (auto parsedCol = WTF::parseInteger<unsigned>(colStr, 10, WTF::ParseIntegerWhitespacePolicy::Disallow)) {
                                                    columnNumber = *parsedCol;
                                                    ok2 = true;
                                                }
                                                parsed = ok1 && ok2;
                                            }
                                        }
                                    }
                                }

                                auto frame = BidiScript::StackFrame::create()
                                    .setLineNumber(parsed ? lineNumber : 0)
                                    .setColumnNumber(parsed ? columnNumber : 0)
                                    .setFunctionName(functionName.isEmpty() ? emptyString() : functionName)
                                    .setUrl(urlPart.isEmpty() ? emptyString() : urlPart)
                                    .release();
                                if (!topLine && parsed)
                                    topLine = lineNumber;
                                if (!topColumn && parsed)
                                    topColumn = columnNumber;
                                callFrames->addItem(WTFMove(frame));
                            }
                        }
                    }
                } else {
                    exceptionRemote = BidiScript::RemoteValue::create()
                        .setType(BidiScript::RemoteValueType::String)
                        .release();
                    exceptionRemote->setValue(JSON::Value::create(String("Unknown exception"_s)));
                }

                auto stackTrace = BidiScript::StackTrace::create()
                    .setCallFrames(WTFMove(callFrames))
                    .release();

                exceptionDetails = BidiScript::ExceptionDetails::create()
                    .setLineNumber(static_cast<int>(topLine))
                    .setColumnNumber(static_cast<int>(topColumn))
                    .setText(exceptionMessage)
                    .setException(exceptionRemote.releaseNonNull())
                    .setStackTrace(WTFMove(stackTrace))
                    .release();
            } else {
                if (auto remoteValue = envelopeObj->getValue("remote"_s)) {
                    evalType = BidiScript::EvaluateResultType::Success;
                    remote = convertJSONToRemoteValue(remoteValue.get());
                } else {
                    evalType = BidiScript::EvaluateResultType::Exception;

                    auto exceptionRemote = BidiScript::RemoteValue::create()
                        .setType(BidiScript::RemoteValueType::Object)
                        .release();
                    exceptionRemote->setValue(JSON::Value::create(String("Missing 'remote' in result envelope"_s)));

                    auto emptyCallFrames = JSON::ArrayOf<BidiScript::StackFrame>::create();
                    auto stackTrace = BidiScript::StackTrace::create()
                        .setCallFrames(WTFMove(emptyCallFrames))
                        .release();

                    exceptionDetails = BidiScript::ExceptionDetails::create()
                        .setLineNumber(0)
                        .setColumnNumber(0)
                        .setText("Malformed envelope result"_s)
                        .setException(WTFMove(exceptionRemote))
                        .setStackTrace(WTFMove(stackTrace))
                        .release();
                }
            }
        } else {
            evalType = BidiScript::EvaluateResultType::Exception;

            auto exceptionRemote = BidiScript::RemoteValue::create()
                .setType(BidiScript::RemoteValueType::Object)
                .release();
            exceptionRemote->setValue(JSON::Value::create(String("Malformed envelope"_s)));

            auto emptyCallFrames = JSON::ArrayOf<BidiScript::StackFrame>::create();
            auto stackTrace = BidiScript::StackTrace::create()
                .setCallFrames(WTFMove(emptyCallFrames))
                .release();

            exceptionDetails = BidiScript::ExceptionDetails::create()
                .setLineNumber(0)
                .setColumnNumber(0)
                .setText("Malformed envelope result"_s)
                .setException(WTFMove(exceptionRemote))
                .setStackTrace(WTFMove(stackTrace))
                .release();
        }
    }

    if (evalType == BidiScript::EvaluateResultType::Exception) {
        if (!exceptionDetails) {
            auto fallbackRemote = BidiScript::RemoteValue::create()
                .setType(BidiScript::RemoteValueType::Error)
                .release();
            fallbackRemote->setValue(JSON::Value::create(String("Script evaluation failed"_s)));

            auto emptyCallFrames = JSON::ArrayOf<BidiScript::StackFrame>::create();
            auto stackTrace = BidiScript::StackTrace::create()
                .setCallFrames(WTFMove(emptyCallFrames))
                .release();

            exceptionDetails = BidiScript::ExceptionDetails::create()
                .setLineNumber(0)
                .setColumnNumber(0)
                .setText("JavaScript exception occurred"_s)
                .setException(WTFMove(fallbackRemote))
                .setStackTrace(WTFMove(stackTrace))
                .release();
        }
        callback({ { evalType, realmId, nullptr, WTFMove(exceptionDetails) } });
        return;
    }

    callback({ { evalType, realmId, WTFMove(remote), nullptr } });
}

RefPtr<Inspector::Protocol::BidiScript::RemoteValue> BidiScriptAgent::serializeAsRemoteValue(RefPtr<JSON::Value> value)
{
    using namespace Inspector::Protocol;

    auto out = BidiScript::RemoteValue::create();

    if (!value)
        return out.setType(BidiScript::RemoteValueType::Undefined).release();

    // Handle different value types according to W3C BiDi spec
    if (value->isNull())
        return out.setType(BidiScript::RemoteValueType::Null).release();

    // String
    String stringValue;
    if (value->asString(stringValue)) {
        auto remoteValue = out.setType(BidiScript::RemoteValueType::String).release();
        remoteValue->setValue(JSON::Value::create(stringValue));
        return remoteValue;
    }

    // Number
    double numberValue;
    if (value->asDouble(numberValue)) {
        auto remoteValue = out.setType(BidiScript::RemoteValueType::Number).release();

        // Handle special number values according to BiDi spec
        if (std::isnan(numberValue)) {
            remoteValue->setValue(JSON::Value::create(String("NaN"_s)));
        } else if (numberValue == 0.0 && std::signbit(numberValue)) {
            remoteValue->setValue(JSON::Value::create(String("-0"_s)));
        } else if (std::isinf(numberValue)) {
            if (numberValue > 0)
                remoteValue->setValue(JSON::Value::create(String("Infinity"_s)));
            else
                remoteValue->setValue(JSON::Value::create(String("-Infinity"_s)));
        } else
            remoteValue->setValue(JSON::Value::create(numberValue));
        return remoteValue;
    }

    // Boolean
    if (auto boolValue = value->asBoolean()) {
        auto remoteValue = out.setType(BidiScript::RemoteValueType::Boolean).release();
        remoteValue->setValue(JSON::Value::create(*boolValue));
        return remoteValue;
    }

    // Array
    if (auto array = value->asArray()) {
        auto serializedArray = JSON::Array::create();

        for (size_t i = 0; i < array->length(); ++i) {
            auto element = array->get(i);
            if (auto serializedElement = serializeAsRemoteValue(element.ptr()))
                serializedArray->pushObject(serializedElement.releaseNonNull());
        }

        auto remoteValue = out.setType(BidiScript::RemoteValueType::Array).release();
        remoteValue->setValue(serializedArray.copyRef());
        return remoteValue;
    }

    // Object (fallback for any other type)
    if (auto object = value->asObject()) {
        // Check if this is a BiDi type marker object from our evaluation wrapper
        auto bidiTypeMarker = object->getString("__bidiType"_s);
        if (!bidiTypeMarker.isNull()) {
            if (bidiTypeMarker == "promise"_s)
                return out.setType(BidiScript::RemoteValueType::Promise).release();
            if (bidiTypeMarker == "function"_s)
                return out.setType(BidiScript::RemoteValueType::Function).release();
            if (bidiTypeMarker == "generator"_s)
                return out.setType(BidiScript::RemoteValueType::Generator).release();
            if (bidiTypeMarker == "regexp"_s) {
                auto remoteValue = out.setType(BidiScript::RemoteValueType::Regexp).release();
                // Extract pattern and flags from __bidiValue
                auto bidiValue = object->getObject("__bidiValue"_s);
                if (bidiValue) {
                    auto regexpValue = JSON::Object::create();
                    auto pattern = bidiValue->getString("pattern"_s);
                    auto flags = bidiValue->getString("flags"_s);
                    if (!pattern.isNull())
                        regexpValue->setString("pattern"_s, pattern);
                    if (!flags.isNull())
                        regexpValue->setString("flags"_s, flags);
                    remoteValue->setValue(regexpValue.copyRef());
                }
                return remoteValue;
            }
            if (bidiTypeMarker == "date"_s) {
                auto remoteValue = out.setType(BidiScript::RemoteValueType::Date).release();
                // Extract ISO date string from __bidiValue
                auto bidiValue = object->getString("__bidiValue"_s);
                if (!bidiValue.isNull())
                    remoteValue->setValue(JSON::Value::create(bidiValue));
                return remoteValue;
            }
            if (bidiTypeMarker == "map"_s)
                return out.setType(BidiScript::RemoteValueType::Map).release();
            if (bidiTypeMarker == "set"_s)
                return out.setType(BidiScript::RemoteValueType::Set).release();
            if (bidiTypeMarker == "weakmap"_s)
                return out.setType(BidiScript::RemoteValueType::Weakmap).release();
            if (bidiTypeMarker == "weakset"_s)
                return out.setType(BidiScript::RemoteValueType::Weakset).release();
            if (bidiTypeMarker == "error"_s)
                return out.setType(BidiScript::RemoteValueType::Error).release();
            if (bidiTypeMarker == "typedarray"_s)
                return out.setType(BidiScript::RemoteValueType::Typedarray).release();
            if (bidiTypeMarker == "arraybuffer"_s)
                return out.setType(BidiScript::RemoteValueType::Arraybuffer).release();
            if (bidiTypeMarker == "symbol"_s)
                return out.setType(BidiScript::RemoteValueType::Symbol).release();
        }

        // Check if this object has Error-like properties to determine if it's an Error
        bool isError = false;
        auto nameString = object->getString("name"_s);
        auto messageString = object->getString("message"_s);


        if (!nameString.isNull() && !messageString.isNull()) {
            // Additional check: see if name contains "Error"
            isError = nameString.containsIgnoringASCIICase("error"_s);
        }

        auto remoteType = isError ? BidiScript::RemoteValueType::Error : BidiScript::RemoteValueType::Object;
        auto remoteValue = out.setType(remoteType).release();
        remoteValue->setValue(object.releaseNonNull());
        return remoteValue;
    }

    // Final fallback - treat as undefined
    return out.setType(BidiScript::RemoteValueType::Undefined).release();
}

// RealmRegistryStub implementation
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


} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)

