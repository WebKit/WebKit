/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#if ENABLE(INSPECTOR)

#include "InspectorState.h"

#include "InspectorStateClient.h"

namespace WebCore {

InspectorState::InspectorState(InspectorStateUpdateListener* listener, PassRefPtr<InspectorObject> properties)
    : m_listener(listener)
    , m_properties(properties)
{
}

void InspectorState::updateCookie()
{
    if (m_listener)
        m_listener->inspectorStateUpdated();
}

void InspectorState::setFromCookie(PassRefPtr<InspectorObject> properties)
{
    m_properties = properties;
}

void InspectorState::setValue(const String& propertyName, PassRefPtr<InspectorValue> value)
{
    m_properties->setValue(propertyName, value);
    updateCookie();
}

void InspectorState::remove(const String& propertyName)
{
    m_properties->remove(propertyName);
    updateCookie();
}

bool InspectorState::getBoolean(const String& propertyName)
{
    InspectorObject::iterator it = m_properties->find(propertyName);
    bool value = false;
    if (it != m_properties->end())
        it->value->asBoolean(&value);
    return value;
}

String InspectorState::getString(const String& propertyName)
{
    InspectorObject::iterator it = m_properties->find(propertyName);
    String value;
    if (it != m_properties->end())
        it->value->asString(&value);
    return value;
}

long InspectorState::getLong(const String& propertyName)
{
    InspectorObject::iterator it = m_properties->find(propertyName);
    long value = 0;
    if (it != m_properties->end())
        it->value->asNumber(&value);
    return value;
}

double InspectorState::getDouble(const String& propertyName)
{
    InspectorObject::iterator it = m_properties->find(propertyName);
    double value = 0;
    if (it != m_properties->end())
        it->value->asNumber(&value);
    return value;
}

PassRefPtr<InspectorObject> InspectorState::getObject(const String& propertyName)
{
    InspectorObject::iterator it = m_properties->find(propertyName);
    if (it == m_properties->end()) {
        m_properties->setObject(propertyName, InspectorObject::create());
        it = m_properties->find(propertyName);
    }
    return it->value->asObject();
}

InspectorState* InspectorCompositeState::createAgentState()
{
    RefPtr<InspectorObject> stateProperties = InspectorObject::create();
    m_stateArray->pushObject(stateProperties);
    m_states.append(adoptPtr(new InspectorState(this, stateProperties)));
    return m_states.last().get();
}

void InspectorCompositeState::loadFromCookie(const String& inspectorCompositeStateCookie)
{
    RefPtr<InspectorValue> cookie = InspectorValue::parseJSON(inspectorCompositeStateCookie);
    if (cookie)
        m_stateArray = cookie->asArray();
    if (!m_stateArray) {
        m_stateArray = InspectorArray::create();
        for (size_t i = 0, size = m_states.size(); i < size; ++i) {
            RefPtr<InspectorObject> stateObject = InspectorObject::create();
            m_stateArray->pushObject(stateObject);
        }
    }

    ASSERT(m_stateArray);
    ASSERT(m_stateArray->length() == m_states.size());
    for (size_t i = 0, size = m_states.size(); i < size; ++i) {
        RefPtr<InspectorValue> stateObject = m_stateArray->get(i);
        m_states.at(i)->setFromCookie(stateObject->asObject());
    }
}

void InspectorCompositeState::mute()
{
    m_isMuted = true;
}

void InspectorCompositeState::unmute()
{
    m_isMuted = false;
}

void InspectorCompositeState::inspectorStateUpdated()
{
    if (m_client && !m_isMuted && m_client->supportsInspectorStateUpdates())
        m_client->updateInspectorStateCookie(m_stateArray->toJSONString());
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
