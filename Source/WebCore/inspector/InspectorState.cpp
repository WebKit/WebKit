/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE AND ITS CONTRIBUTORS "AS IS" AND ANY
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
#include "InspectorState.h"

#if ENABLE(INSPECTOR)

#include "InspectorClient.h"

namespace WebCore {

InspectorState::InspectorState(InspectorClient* client)
    : m_client(client)
{
    // Pure reload state
    registerBoolean(userInitiatedProfiling, false, String());
    registerBoolean(timelineProfilerEnabled, false, String());
    registerBoolean(searchingForNode, false, String());
    registerObject(stickyBreakpoints, String());

    // Should go away
    registerBoolean(consoleMessagesEnabled, false, "consoleMessagesEnabled");
    registerBoolean(monitoringXHR, false, "monitoringXHREnabled");
    registerLong(pauseOnExceptionsState, 0, "pauseOnExceptionsState");
}

void InspectorState::restoreFromInspectorCookie(const String& json)
{
    RefPtr<InspectorValue> jsonValue = InspectorValue::parseJSON(json);
    if (!jsonValue)
        return;

    RefPtr<InspectorObject> jsonObject = jsonValue->asObject();
    if (!jsonObject)
        return;

    for (InspectorObject::iterator i = jsonObject->begin(); i != jsonObject->end(); ++i) {
        InspectorPropertyId id = (InspectorPropertyId)i->first.toInt();
        ASSERT(id > 0 && id < lastPropertyId);
        PropertyMap::iterator j = m_properties.find(id);
        ASSERT(j != m_properties.end());
        ASSERT(j->second.m_value->type() == i->second->type());
        j->second.m_value = i->second;
    }
}

PassRefPtr<InspectorObject> InspectorState::generateStateObjectForFrontend()
{
    RefPtr<InspectorObject> stateObject = InspectorObject::create();
    for (PropertyMap::iterator i = m_properties.begin(); i != m_properties.end(); ++i) {
        if (i->second.m_frontendAlias.length())
            stateObject->setValue(i->second.m_frontendAlias, i->second.m_value);
    }
    return stateObject.release();
}

void InspectorState::updateCookie()
{
    RefPtr<InspectorObject> cookieObject = InspectorObject::create();
    for (PropertyMap::iterator i = m_properties.begin(); i != m_properties.end(); ++i)
        cookieObject->setValue(String::number(i->first), i->second.m_value);
    m_client->updateInspectorStateCookie(cookieObject->toJSONString());
}

void InspectorState::setValue(InspectorPropertyId id, PassRefPtr<InspectorValue> value)
{
    PropertyMap::iterator i = m_properties.find(id);
    ASSERT(i != m_properties.end());
    i->second.m_value = value;
    updateCookie();
}

bool InspectorState::getBoolean(InspectorPropertyId id)
{
    PropertyMap::iterator i = m_properties.find(id);
    ASSERT(i != m_properties.end());
    bool value = false;
    i->second.m_value->asBoolean(&value);
    return value;
}

String InspectorState::getString(InspectorPropertyId id)
{
    PropertyMap::iterator i = m_properties.find(id);
    ASSERT(i != m_properties.end());
    String value;
    i->second.m_value->asString(&value);
    return value;
}

long InspectorState::getLong(InspectorPropertyId id)
{
    PropertyMap::iterator i = m_properties.find(id);
    ASSERT(i != m_properties.end());
    long value = 0;
    i->second.m_value->asNumber(&value);
    return value;
}

PassRefPtr<InspectorObject> InspectorState::getObject(InspectorPropertyId id)
{
    PropertyMap::iterator i = m_properties.find(id);
    ASSERT(i != m_properties.end());
    return i->second.m_value->asObject();
}

void InspectorState::setObject(InspectorPropertyId id, PassRefPtr<InspectorObject> value)
{
    PropertyMap::iterator i = m_properties.find(id);
    ASSERT(i != m_properties.end());
    Property& property = i->second;
    property.m_value = value;
    updateCookie();
}

void InspectorState::registerBoolean(InspectorPropertyId propertyId, bool value, const String& frontendAlias)
{
    m_properties.set(propertyId, Property::create(InspectorBasicValue::create(value), frontendAlias));
}

void InspectorState::registerString(InspectorPropertyId propertyId, const String& value, const String& frontendAlias)
{
    m_properties.set(propertyId, Property::create(InspectorString::create(value), frontendAlias));
}

void InspectorState::registerLong(InspectorPropertyId propertyId, long value, const String& frontendAlias)
{
    m_properties.set(propertyId, Property::create(InspectorBasicValue::create((double)value), frontendAlias));
}

void InspectorState::registerObject(InspectorPropertyId propertyId, const String& frontendAlias)
{
    m_properties.set(propertyId, Property::create(InspectorObject::create(), frontendAlias));
}

InspectorState::Property InspectorState::Property::create(PassRefPtr<InspectorValue> value, const String& frontendAlias)
{
    Property property;
    property.m_value = value;
    property.m_frontendAlias = frontendAlias;
    return property;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
