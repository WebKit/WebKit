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

#ifndef InspectorState_h
#define InspectorState_h

#if ENABLE(INSPECTOR)

#include "InspectorValues.h"
#include "PlatformString.h"

#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class InspectorClient;

class InspectorState {
public:
    enum InspectorPropertyId {
        monitoringXHR = 1,
        timelineProfilerEnabled,
        searchingForNode,
        consoleMessagesEnabled,
        userInitiatedProfiling,
        browserBreakpoints,
        resourceAgentEnabled,
        lastPropertyId
    };

    InspectorState(InspectorClient* client);

    void restoreFromInspectorCookie(const String& jsonString);

    bool getBoolean(InspectorPropertyId);
    String getString(InspectorPropertyId);
    long getLong(InspectorPropertyId);
    PassRefPtr<InspectorObject> getObject(InspectorPropertyId);

    void setBoolean(InspectorPropertyId propertyId, bool value) { setValue(propertyId, InspectorBasicValue::create(value)); }
    void setString(InspectorPropertyId propertyId, const String& value) { setValue(propertyId, InspectorString::create(value)); }
    void setLong(InspectorPropertyId propertyId, long value) { setValue(propertyId, InspectorBasicValue::create((double)value)); }
    void setObject(InspectorPropertyId, PassRefPtr<InspectorObject> value);

private:
    void updateCookie();
    void setValue(InspectorPropertyId, PassRefPtr<InspectorValue>);

    struct Property {
        static Property create(PassRefPtr<InspectorValue> value);
        RefPtr<InspectorValue> m_value;
    };
    typedef HashMap<long, Property> PropertyMap;
    PropertyMap m_properties;

    void registerBoolean(InspectorPropertyId, bool value);
    void registerString(InspectorPropertyId, const String& value);
    void registerLong(InspectorPropertyId, long value);
    void registerObject(InspectorPropertyId);

    InspectorClient* m_client;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
#endif // !defined(InspectorState_h)
