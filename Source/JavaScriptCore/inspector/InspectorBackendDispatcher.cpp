/*
 * Copyright (C) 2013-2017 Apple Inc. All Rights Reserved.
 * Copyright (C) 2011 The Chromium Authors. All rights reserved.
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
#include "InspectorBackendDispatcher.h"

#include "InspectorFrontendRouter.h"
#include <wtf/JSONValues.h>
#include <wtf/SetForScope.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

SupplementalBackendDispatcher::SupplementalBackendDispatcher(BackendDispatcher& backendDispatcher)
    : m_backendDispatcher(backendDispatcher)
{
}

SupplementalBackendDispatcher::~SupplementalBackendDispatcher()
{
}

BackendDispatcher::CallbackBase::CallbackBase(Ref<BackendDispatcher>&& backendDispatcher, long requestId)
    : m_backendDispatcher(WTFMove(backendDispatcher))
    , m_requestId(requestId)
{
}

bool BackendDispatcher::CallbackBase::isActive() const
{
    return !m_alreadySent && m_backendDispatcher->isActive();
}

void BackendDispatcher::CallbackBase::sendFailure(const ErrorString& error)
{
    ASSERT(error.length());

    if (m_alreadySent)
        return;

    m_alreadySent = true;

    // Immediately send an error message since this is an async response with a single error.
    m_backendDispatcher->reportProtocolError(m_requestId, ServerError, error);
    m_backendDispatcher->sendPendingErrors();
}

void BackendDispatcher::CallbackBase::sendSuccess(Ref<JSON::Object>&& partialMessage)
{
    if (m_alreadySent)
        return;

    m_alreadySent = true;
    m_backendDispatcher->sendResponse(m_requestId, WTFMove(partialMessage), false);
}

BackendDispatcher::BackendDispatcher(Ref<FrontendRouter>&& router)
    : m_frontendRouter(WTFMove(router))
{
}

Ref<BackendDispatcher> BackendDispatcher::create(Ref<FrontendRouter>&& router)
{
    return adoptRef(*new BackendDispatcher(WTFMove(router)));
}

bool BackendDispatcher::isActive() const
{
    return m_frontendRouter->hasFrontends();
}

void BackendDispatcher::registerDispatcherForDomain(const String& domain, SupplementalBackendDispatcher* dispatcher)
{
    ASSERT_ARG(dispatcher, dispatcher);

    // FIXME: <https://webkit.org/b/148492> Agents should only register with the backend once,
    // and we should re-add the assertion that only one dispatcher is registered per domain.
    m_dispatchers.set(domain, dispatcher);
}

void BackendDispatcher::dispatch(const String& message)
{
    Ref<BackendDispatcher> protect(*this);

    ASSERT(!m_protocolErrors.size());

    int requestId = 0;
    RefPtr<JSON::Object> messageObject;

    {
        // In case this is a re-entrant call from a nested run loop, we don't want to lose
        // the outer request's id just because the inner request is bogus.
        SetForScope<Optional<long>> scopedRequestId(m_currentRequestId, WTF::nullopt);

        auto parsedMessage = JSON::Value::parseJSON(message);
        if (!parsedMessage) {
            reportProtocolError(ParseError, "Message must be in JSON format"_s);
            sendPendingErrors();
            return;
        }

        messageObject = parsedMessage->asObject();
        if (!messageObject) {
            reportProtocolError(InvalidRequest, "Message must be a JSONified object"_s);
            sendPendingErrors();
            return;
        }

        auto requestIdValue = messageObject->getValue("id"_s);
        if (!requestIdValue) {
            reportProtocolError(InvalidRequest, "'id' property was not found"_s);
            sendPendingErrors();
            return;
        }

        auto requestIdInt = requestIdValue->asInteger();
        if (!requestIdInt) {
            reportProtocolError(InvalidRequest, "The type of 'id' property must be integer"_s);
            sendPendingErrors();
            return;
        }

        requestId = *requestIdInt;
    }

    {
        // We could be called re-entrantly from a nested run loop, so restore the previous id.
        SetForScope<Optional<long>> scopedRequestId(m_currentRequestId, requestId);

        auto methodValue = messageObject->getValue("method"_s);
        if (!methodValue) {
            reportProtocolError(InvalidRequest, "'method' property wasn't found"_s);
            sendPendingErrors();
            return;
        }

        auto methodString = methodValue->asString();
        if (!methodString) {
            reportProtocolError(InvalidRequest, "The type of 'method' property must be string"_s);
            sendPendingErrors();
            return;
        }

        Vector<String> domainAndMethod = methodString.splitAllowingEmptyEntries('.');
        if (domainAndMethod.size() != 2 || !domainAndMethod[0].length() || !domainAndMethod[1].length()) {
            reportProtocolError(InvalidRequest, "The 'method' property was formatted incorrectly. It should be 'Domain.method'"_s);
            sendPendingErrors();
            return;
        }

        String domain = domainAndMethod[0];
        SupplementalBackendDispatcher* domainDispatcher = m_dispatchers.get(domain);
        if (!domainDispatcher) {
            reportProtocolError(MethodNotFound, "'" + domain + "' domain was not found");
            sendPendingErrors();
            return;
        }

        String method = domainAndMethod[1];
        domainDispatcher->dispatch(requestId, method, messageObject.releaseNonNull());

        if (m_protocolErrors.size())
            sendPendingErrors();
    }
}

// FIXME: <http://webkit.org/b/179847> remove this function when legacy InspectorObject symbols are no longer needed.
void BackendDispatcher::sendResponse(long requestId, RefPtr<JSON::Object>&& result)
{
    ASSERT(result);
    sendResponse(requestId, result.releaseNonNull(), false);
}

// FIXME: <http://webkit.org/b/179847> remove this function when legacy InspectorObject symbols are no longer needed.
void BackendDispatcher::sendResponse(long requestId, RefPtr<JSON::Object>&& result, bool)
{
    ASSERT(result);
    sendResponse(requestId, result.releaseNonNull(), false);
}

// FIXME: <http://webkit.org/b/179847> remove this function when legacy InspectorObject symbols are no longer needed.
void BackendDispatcher::sendResponse(long requestId, Ref<JSON::Object>&& result)
{
    sendResponse(requestId, WTFMove(result), false);
}

void BackendDispatcher::sendResponse(long requestId, Ref<JSON::Object>&& result, bool)
{
    ASSERT(!m_protocolErrors.size());

    // The JSON-RPC 2.0 specification requires that the "error" member have the value 'null'
    // if no error occurred during an invocation, but we do not include it at all.
    Ref<JSON::Object> responseMessage = JSON::Object::create();
    responseMessage->setObject("result"_s, WTFMove(result));
    responseMessage->setInteger("id"_s, requestId);
    m_frontendRouter->sendResponse(responseMessage->toJSONString());
}

void BackendDispatcher::sendPendingErrors()
{
    // These error codes are specified in JSON-RPC 2.0, Section 5.1.
    static const int errorCodes[] = {
        -32700, // ParseError
        -32600, // InvalidRequest
        -32601, // MethodNotFound
        -32602, // InvalidParams
        -32603, // InternalError
        -32000, // ServerError
    };

    // To construct the error object, only use the last error's code and message.
    // Per JSON-RPC 2.0, Section 5.1, the 'data' member may contain nested errors,
    // but only one top-level Error object should be sent per request.
    CommonErrorCode errorCode = InternalError;
    String errorMessage;
    Ref<JSON::Array> payload = JSON::Array::create();
    
    for (auto& data : m_protocolErrors) {
        errorCode = std::get<0>(data);
        errorMessage = std::get<1>(data);

        ASSERT_ARG(errorCode, (unsigned)errorCode < WTF_ARRAY_LENGTH(errorCodes));
        ASSERT_ARG(errorCode, errorCodes[errorCode]);

        Ref<JSON::Object> error = JSON::Object::create();
        error->setInteger("code"_s, errorCodes[errorCode]);
        error->setString("message"_s, errorMessage);
        payload->pushObject(WTFMove(error));
    }

    Ref<JSON::Object> topLevelError = JSON::Object::create();
    topLevelError->setInteger("code"_s, errorCodes[errorCode]);
    topLevelError->setString("message"_s, errorMessage);
    topLevelError->setArray("data"_s, WTFMove(payload));

    Ref<JSON::Object> message = JSON::Object::create();
    message->setObject("error"_s, WTFMove(topLevelError));
    if (m_currentRequestId)
        message->setInteger("id"_s, m_currentRequestId.value());
    else {
        // The 'null' value for an unknown id is specified in JSON-RPC 2.0, Section 5.
        message->setValue("id"_s, JSON::Value::null());
    }

    m_frontendRouter->sendResponse(message->toJSONString());

    m_protocolErrors.clear();
    m_currentRequestId = WTF::nullopt;
}
    
void BackendDispatcher::reportProtocolError(CommonErrorCode errorCode, const String& errorMessage)
{
    reportProtocolError(m_currentRequestId, errorCode, errorMessage);
}

void BackendDispatcher::reportProtocolError(Optional<long> relatedRequestId, CommonErrorCode errorCode, const String& errorMessage)
{
    ASSERT_ARG(errorCode, errorCode >= 0);

    // If the error was reported from an async callback, then no request id will be registered yet.
    if (!m_currentRequestId)
        m_currentRequestId = relatedRequestId;

    m_protocolErrors.append(std::tuple<CommonErrorCode, String>(errorCode, errorMessage));
}

template<typename T>
T BackendDispatcher::getPropertyValue(JSON::Object* params, const String& name, bool required, std::function<T(JSON::Value&)> converter, const char* typeName)
{
    T result;

    if (!params) {
        if (required)
            reportProtocolError(BackendDispatcher::InvalidParams, makeString("'params' object must contain required parameter '", name, "' with type '", typeName, "'."));
        return result;
    }

    auto findResult = params->find(name);
    if (findResult == params->end()) {
        if (required)
            reportProtocolError(BackendDispatcher::InvalidParams, makeString("Parameter '", name, "' with type '", typeName, "' was not found."));
        return result;
    }

    result = converter(findResult->value);

    if (!result)
        reportProtocolError(BackendDispatcher::InvalidParams, makeString("Parameter '", name, "' has wrong type. It must be '", typeName, "'."));

    return result;
}

Optional<bool> BackendDispatcher::getBoolean(JSON::Object* params, const String& name, bool required)
{
    return getPropertyValue<Optional<bool>>(params, name, required, &JSON::Value::asBoolean, "Boolean");
}

Optional<int> BackendDispatcher::getInteger(JSON::Object* params, const String& name, bool required)
{
    // FIXME: <http://webkit.org/b/179847> simplify this when legacy InspectorObject symbols are no longer needed.
    Optional<int> (JSON::Value::*asInteger)() const = &JSON::Value::asInteger;
    return getPropertyValue<Optional<int>>(params, name, required, asInteger, "Integer");
}

Optional<double> BackendDispatcher::getDouble(JSON::Object* params, const String& name, bool required)
{
    // FIXME: <http://webkit.org/b/179847> simplify this when legacy InspectorObject symbols are no longer needed.
    Optional<double> (JSON::Value::*asDouble)() const = &JSON::Value::asDouble;
    return getPropertyValue<Optional<double>>(params, name, required, asDouble, "Number");
}

String BackendDispatcher::getString(JSON::Object* params, const String& name, bool required)
{
    // FIXME: <http://webkit.org/b/179847> simplify this when legacy InspectorObject symbols are no longer needed.
    String (JSON::Value::*asString)() const = &JSON::Value::asString;
    return getPropertyValue<String>(params, name, required, asString, "String");
}

RefPtr<JSON::Value> BackendDispatcher::getValue(JSON::Object* params, const String& name, bool required)
{
    return getPropertyValue<RefPtr<JSON::Value>>(params, name, required, &JSON::Value::asValue, "Value");
}

RefPtr<JSON::Object> BackendDispatcher::getObject(JSON::Object* params, const String& name, bool required)
{
    return getPropertyValue<RefPtr<JSON::Object>>(params, name, required, &JSON::Value::asObject, "Object");
}

RefPtr<JSON::Array> BackendDispatcher::getArray(JSON::Object* params, const String& name, bool required)
{
    return getPropertyValue<RefPtr<JSON::Array>>(params, name, required, &JSON::Value::asArray, "Array");
}

} // namespace Inspector
