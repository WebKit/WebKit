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

#if ENABLE(INSPECTOR)

#include "InspectorFrontendChannel.h"
#include "InspectorValues.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

InspectorBackendDispatcher::CallbackBase::CallbackBase(PassRefPtr<InspectorBackendDispatcher> backendDispatcher, int id)
    : m_backendDispatcher(backendDispatcher)
    , m_id(id)
    , m_alreadySent(false)
{
}

bool InspectorBackendDispatcher::CallbackBase::isActive() const
{
    return !m_alreadySent && m_backendDispatcher->isActive();
}

void InspectorBackendDispatcher::CallbackBase::sendFailure(const ErrorString& error)
{
    ASSERT(error.length());
    sendIfActive(nullptr, error);
}

void InspectorBackendDispatcher::CallbackBase::sendIfActive(PassRefPtr<InspectorObject> partialMessage, const ErrorString& invocationError)
{
    if (m_alreadySent)
        return;

    m_backendDispatcher->sendResponse(m_id, partialMessage, invocationError);
    m_alreadySent = true;
}

PassRefPtr<InspectorBackendDispatcher> InspectorBackendDispatcher::create(InspectorFrontendChannel* inspectorFrontendChannel)
{
    return adoptRef(new InspectorBackendDispatcher(inspectorFrontendChannel));
}

void InspectorBackendDispatcher::registerDispatcherForDomain(const String& domain, InspectorSupplementalBackendDispatcher* dispatcher)
{
    auto result = m_dispatchers.add(domain, dispatcher);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void InspectorBackendDispatcher::dispatch(const String& message)
{
    Ref<InspectorBackendDispatcher> protect(*this);

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

    RefPtr<InspectorValue> callIdValue = messageObject->get(ASCIILiteral("id"));
    if (!callIdValue) {
        reportProtocolError(nullptr, InvalidRequest, ASCIILiteral("'id' property was not found"));
        return;
    }

    long callId = 0;
    if (!callIdValue->asInteger(callId)) {
        reportProtocolError(nullptr, InvalidRequest, ASCIILiteral("The type of 'id' property must be integer"));
        return;
    }

    RefPtr<InspectorValue> methodValue = messageObject->get(ASCIILiteral("method"));
    if (!methodValue) {
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
    InspectorSupplementalBackendDispatcher* domainDispatcher = m_dispatchers.get(domain);
    if (!domainDispatcher) {
        reportProtocolError(&callId, MethodNotFound, "'" + domain + "' domain was not found");
        return;
    }

    String domainMethod = method.substring(position + 1);
    domainDispatcher->dispatch(callId, domainMethod, messageObject.release());
}

void InspectorBackendDispatcher::sendResponse(long callId, PassRefPtr<InspectorObject> result, const ErrorString& invocationError)
{
    if (!m_inspectorFrontendChannel)
        return;

    if (invocationError.length()) {
        reportProtocolError(&callId, ServerError, invocationError);
        return;
    }

    RefPtr<InspectorObject> responseMessage = InspectorObject::create();
    responseMessage->setObject(ASCIILiteral("result"), result);
    responseMessage->setInteger(ASCIILiteral("id"), callId);
    m_inspectorFrontendChannel->sendMessageToFrontend(responseMessage->toJSONString());
}

void InspectorBackendDispatcher::reportProtocolError(const long* const callId, CommonErrorCode errorCode, const String& errorMessage) const
{
    reportProtocolError(callId, errorCode, errorMessage, nullptr);
}

void InspectorBackendDispatcher::reportProtocolError(const long* const callId, CommonErrorCode errorCode, const String& errorMessage, PassRefPtr<InspectorArray> data) const
{
    static const int errorCodes[] = {
        -32700, // ParseError
        -32600, // InvalidRequest
        -32601, // MethodNotFound
        -32602, // InvalidParams
        -32603, // InternalError
        -32000, // ServerError
    };

    ASSERT(errorCode >= 0);
    ASSERT((unsigned)errorCode < WTF_ARRAY_LENGTH(errorCodes));
    ASSERT(errorCodes[errorCode]);

    if (!m_inspectorFrontendChannel)
        return;

    RefPtr<InspectorObject> error = InspectorObject::create();
    error->setInteger(ASCIILiteral("code"), errorCodes[errorCode]);
    error->setString(ASCIILiteral("message"), errorMessage);
    if (data)
        error->setArray(ASCIILiteral("data"), data);

    RefPtr<InspectorObject> message = InspectorObject::create();
    message->setObject(ASCIILiteral("error"), error.release());
    if (callId)
        message->setInteger(ASCIILiteral("id"), *callId);
    else
        message->setValue(ASCIILiteral("id"), InspectorValue::null());

    m_inspectorFrontendChannel->sendMessageToFrontend(message->toJSONString());
}

template<typename ReturnValueType, typename ValueType, typename DefaultValueType>
static ReturnValueType getPropertyValue(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors, DefaultValueType defaultValue, bool (*asMethod)(InspectorValue&, ValueType&), const char* typeName)
{
    ASSERT(protocolErrors);

    ValueType value = defaultValue;
    if (valueFound)
        *valueFound = false;

    if (!object) {
        if (!valueFound)
            protocolErrors->pushString(String::format("'params' object must contain required parameter '%s' with type '%s'.", name.utf8().data(), typeName));
        return value;
    }

    InspectorObject::const_iterator end = object->end();
    InspectorObject::const_iterator valueIterator = object->find(name);
    if (valueIterator == end) {
        if (!valueFound)
            protocolErrors->pushString(String::format("Parameter '%s' with type '%s' was not found.", name.utf8().data(), typeName));
        return value;
    }

    if (!asMethod(*valueIterator->value, value)) {
        protocolErrors->pushString(String::format("Parameter '%s' has wrong type. It must be '%s'.", name.utf8().data(), typeName));
        return value;
    }

    if (valueFound)
        *valueFound = true;

    return value;
}

struct AsMethodBridges {
    static bool asInteger(InspectorValue& value, int& output) { return value.asInteger(output); }
    static bool asDouble(InspectorValue& value, double& output) { return value.asDouble(output); }
    static bool asString(InspectorValue& value, String& output) { return value.asString(output); }
    static bool asBoolean(InspectorValue& value, bool& output) { return value.asBoolean(output); }
    static bool asObject(InspectorValue& value, RefPtr<InspectorObject>& output) { return value.asObject(output); }
    static bool asArray(InspectorValue& value, RefPtr<InspectorArray>& output) { return value.asArray(output); }
};

int InspectorBackendDispatcher::getInteger(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors)
{
    return getPropertyValue<int, int, int>(object, name, valueFound, protocolErrors, 0, AsMethodBridges::asInteger, "Integer");
}

double InspectorBackendDispatcher::getDouble(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors)
{
    return getPropertyValue<double, double, double>(object, name, valueFound, protocolErrors, 0, AsMethodBridges::asDouble, "Number");
}

String InspectorBackendDispatcher::getString(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors)
{
    return getPropertyValue<String, String, String>(object, name, valueFound, protocolErrors, "", AsMethodBridges::asString, "String");
}

bool InspectorBackendDispatcher::getBoolean(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors)
{
    return getPropertyValue<bool, bool, bool>(object, name, valueFound, protocolErrors, false, AsMethodBridges::asBoolean, "Boolean");
}

PassRefPtr<InspectorObject> InspectorBackendDispatcher::getObject(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors)
{
    return getPropertyValue<PassRefPtr<InspectorObject>, RefPtr<InspectorObject>, InspectorObject*>(object, name, valueFound, protocolErrors, nullptr, AsMethodBridges::asObject, "Object");
}

PassRefPtr<InspectorArray> InspectorBackendDispatcher::getArray(InspectorObject* object, const String& name, bool* valueFound, InspectorArray* protocolErrors)
{
    return getPropertyValue<PassRefPtr<InspectorArray>, RefPtr<InspectorArray>, InspectorArray*>(object, name, valueFound, protocolErrors, nullptr, AsMethodBridges::asArray, "Array");
}

} // namespace Inspector

#endif // ENABLE(INSPECTOR)
