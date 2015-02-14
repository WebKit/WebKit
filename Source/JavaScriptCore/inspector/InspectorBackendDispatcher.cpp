/*
 * Copyright (C) 2013 Apple Inc. All Rights Reserved.
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

#include "InspectorFrontendChannel.h"
#include "InspectorValues.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

BackendDispatcher::CallbackBase::CallbackBase(Ref<BackendDispatcher>&& backendDispatcher, int id)
    : m_backendDispatcher(WTF::move(backendDispatcher))
    , m_id(id)
    , m_alreadySent(false)
{
}

bool BackendDispatcher::CallbackBase::isActive() const
{
    return !m_alreadySent && m_backendDispatcher->isActive();
}

void BackendDispatcher::CallbackBase::sendFailure(const ErrorString& error)
{
    ASSERT(error.length());
    sendIfActive(nullptr, error);
}

void BackendDispatcher::CallbackBase::sendIfActive(RefPtr<InspectorObject>&& partialMessage, const ErrorString& invocationError)
{
    if (m_alreadySent)
        return;

    m_backendDispatcher->sendResponse(m_id, WTF::move(partialMessage), invocationError);
    m_alreadySent = true;
}

Ref<BackendDispatcher> BackendDispatcher::create(FrontendChannel* frontendChannel)
{
    return adoptRef(*new BackendDispatcher(frontendChannel));
}

void BackendDispatcher::registerDispatcherForDomain(const String& domain, SupplementalBackendDispatcher* dispatcher)
{
    auto result = m_dispatchers.add(domain, dispatcher);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void BackendDispatcher::dispatch(const String& message)
{
    Ref<BackendDispatcher> protect(*this);

    RefPtr<InspectorValue> parsedMessage;
    if (!InspectorValue::parseJSON(message, parsedMessage)) {
        reportProtocolError(nullptr, ParseError, ASCIILiteral("Message must be in JSON format"));
        return;
    }

    RefPtr<InspectorObject> messageObject;
    if (!parsedMessage->asObject(messageObject)) {
        reportProtocolError(nullptr, InvalidRequest, ASCIILiteral("Message must be a JSONified object"));
        return;
    }

    RefPtr<InspectorValue> callIdValue;
    if (!messageObject->getValue(ASCIILiteral("id"), callIdValue)) {
        reportProtocolError(nullptr, InvalidRequest, ASCIILiteral("'id' property was not found"));
        return;
    }

    long callId = 0;
    if (!callIdValue->asInteger(callId)) {
        reportProtocolError(nullptr, InvalidRequest, ASCIILiteral("The type of 'id' property must be integer"));
        return;
    }

    RefPtr<InspectorValue> methodValue;
    if (!messageObject->getValue(ASCIILiteral("method"), methodValue)) {
        reportProtocolError(&callId, InvalidRequest, ASCIILiteral("'method' property wasn't found"));
        return;
    }

    String method;
    if (!methodValue->asString(method)) {
        reportProtocolError(&callId, InvalidRequest, ASCIILiteral("The type of 'method' property must be string"));
        return;
    }

    size_t position = method.find('.');
    if (position == WTF::notFound) {
        reportProtocolError(&callId, InvalidRequest, ASCIILiteral("The 'method' property was formatted incorrectly. It should be 'Domain.method'"));
        return;
    }

    String domain = method.substring(0, position);
    SupplementalBackendDispatcher* domainDispatcher = m_dispatchers.get(domain);
    if (!domainDispatcher) {
        reportProtocolError(&callId, MethodNotFound, "'" + domain + "' domain was not found");
        return;
    }

    String domainMethod = method.substring(position + 1);
    domainDispatcher->dispatch(callId, domainMethod, messageObject.releaseNonNull());
}

void BackendDispatcher::sendResponse(long callId, RefPtr<InspectorObject>&& result, const ErrorString& invocationError)
{
    if (!m_frontendChannel)
        return;

    if (invocationError.length()) {
        reportProtocolError(&callId, ServerError, invocationError);
        return;
    }

    Ref<InspectorObject> responseMessage = InspectorObject::create();
    responseMessage->setObject(ASCIILiteral("result"), result);
    responseMessage->setInteger(ASCIILiteral("id"), callId);
    m_frontendChannel->sendMessageToFrontend(responseMessage->toJSONString());
}

void BackendDispatcher::reportProtocolError(const long* const callId, CommonErrorCode errorCode, const String& errorMessage) const
{
    reportProtocolError(callId, errorCode, errorMessage, nullptr);
}

void BackendDispatcher::reportProtocolError(const long* const callId, CommonErrorCode errorCode, const String& errorMessage, RefPtr<Inspector::Protocol::Array<String>>&& data) const
{
    static const int errorCodes[] = {
        -32700, // ParseError
        -32600, // InvalidRequest
        -32601, // MethodNotFound
        -32602, // InvalidParams
        -32603, // InternalError
        -32000, // ServerError
    };

    ASSERT_ARG(errorCode, errorCode >= 0);
    ASSERT_ARG(errorCode, (unsigned)errorCode < WTF_ARRAY_LENGTH(errorCodes));
    ASSERT_ARG(errorCode, errorCodes[errorCode]);

    if (!m_frontendChannel)
        return;

    Ref<InspectorObject> error = InspectorObject::create();
    error->setInteger(ASCIILiteral("code"), errorCodes[errorCode]);
    error->setString(ASCIILiteral("message"), errorMessage);
    if (data)
        error->setArray(ASCIILiteral("data"), WTF::move(data));

    Ref<InspectorObject> message = InspectorObject::create();
    message->setObject(ASCIILiteral("error"), WTF::move(error));
    if (callId)
        message->setInteger(ASCIILiteral("id"), *callId);
    else
        message->setValue(ASCIILiteral("id"), InspectorValue::null());

    m_frontendChannel->sendMessageToFrontend(message->toJSONString());
}

template<typename ReturnValueType, typename ValueType, typename DefaultValueType>
static ReturnValueType getPropertyValue(InspectorObject* object, const String& name, bool* out_optionalValueFound, Inspector::Protocol::Array<String>& protocolErrors, DefaultValueType defaultValue, bool (*asMethod)(InspectorValue&, ValueType&), const char* typeName)
{
    ValueType result = defaultValue;
    // out_optionalValueFound signals to the caller whether an optional property was found.
    // if out_optionalValueFound == nullptr, then this is a required property.
    if (out_optionalValueFound)
        *out_optionalValueFound = false;

    if (!object) {
        if (!out_optionalValueFound)
            protocolErrors.addItem(String::format("'params' object must contain required parameter '%s' with type '%s'.", name.utf8().data(), typeName));
        return result;
    }

    auto findResult = object->find(name);
    if (findResult == object->end()) {
        if (!out_optionalValueFound)
            protocolErrors.addItem(String::format("Parameter '%s' with type '%s' was not found.", name.utf8().data(), typeName));
        return result;
    }

    if (!asMethod(*findResult->value, result)) {
        protocolErrors.addItem(String::format("Parameter '%s' has wrong type. It must be '%s'.", name.utf8().data(), typeName));
        return result;
    }

    if (out_optionalValueFound)
        *out_optionalValueFound = true;

    return result;
}

struct AsMethodBridges {
    static bool asInteger(InspectorValue& value, int& output) { return value.asInteger(output); }
    static bool asDouble(InspectorValue& value, double& output) { return value.asDouble(output); }
    static bool asString(InspectorValue& value, String& output) { return value.asString(output); }
    static bool asBoolean(InspectorValue& value, bool& output) { return value.asBoolean(output); }
    static bool asObject(InspectorValue& value, RefPtr<InspectorObject>& output) { return value.asObject(output); }
    static bool asArray(InspectorValue& value, RefPtr<InspectorArray>& output) { return value.asArray(output); }
    static bool asValue(InspectorValue& value, RefPtr<InspectorValue>& output) { return value.asValue(output); }
};

int BackendDispatcher::getInteger(InspectorObject* object, const String& name, bool* valueFound, Inspector::Protocol::Array<String>& protocolErrors)
{
    return getPropertyValue<int, int, int>(object, name, valueFound, protocolErrors, 0, AsMethodBridges::asInteger, "Integer");
}

double BackendDispatcher::getDouble(InspectorObject* object, const String& name, bool* valueFound, Inspector::Protocol::Array<String>& protocolErrors)
{
    return getPropertyValue<double, double, double>(object, name, valueFound, protocolErrors, 0, AsMethodBridges::asDouble, "Number");
}

String BackendDispatcher::getString(InspectorObject* object, const String& name, bool* valueFound, Inspector::Protocol::Array<String>& protocolErrors)
{
    return getPropertyValue<String, String, String>(object, name, valueFound, protocolErrors, "", AsMethodBridges::asString, "String");
}

bool BackendDispatcher::getBoolean(InspectorObject* object, const String& name, bool* valueFound, Inspector::Protocol::Array<String>& protocolErrors)
{
    return getPropertyValue<bool, bool, bool>(object, name, valueFound, protocolErrors, false, AsMethodBridges::asBoolean, "Boolean");
}

RefPtr<InspectorObject> BackendDispatcher::getObject(InspectorObject* object, const String& name, bool* valueFound, Inspector::Protocol::Array<String>& protocolErrors)
{
    return getPropertyValue<RefPtr<InspectorObject>, RefPtr<InspectorObject>, InspectorObject*>(object, name, valueFound, protocolErrors, nullptr, AsMethodBridges::asObject, "Object");
}

RefPtr<InspectorArray> BackendDispatcher::getArray(InspectorObject* object, const String& name, bool* valueFound, Inspector::Protocol::Array<String>& protocolErrors)
{
    return getPropertyValue<RefPtr<InspectorArray>, RefPtr<InspectorArray>, InspectorArray*>(object, name, valueFound, protocolErrors, nullptr, AsMethodBridges::asArray, "Array");
}

RefPtr<InspectorValue> BackendDispatcher::getValue(InspectorObject* object, const String& name, bool* valueFound, Inspector::Protocol::Array<String>& protocolErrors)
{
    return getPropertyValue<RefPtr<InspectorValue>, RefPtr<InspectorValue>, InspectorValue*>(object, name, valueFound, protocolErrors, nullptr, AsMethodBridges::asValue, "Value");
}

} // namespace Inspector
