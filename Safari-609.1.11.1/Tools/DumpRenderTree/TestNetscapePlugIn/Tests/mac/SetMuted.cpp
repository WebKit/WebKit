/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "PluginTest.h"

using namespace std;

class SetMuted : public PluginTest {
public:
    SetMuted(NPP npp, const string& identifier)
        : PluginTest(npp, identifier)
        , m_cachedIsMuted(false)
    {
    }

private:
    bool isMuted()
    {
        NPBool muted = FALSE;
        NPN_GetValue(NPNVmuteAudioBool, &muted);
        return muted;
    }

    bool cachedIsMuted()
    {
        return m_cachedIsMuted;
    }

    class ScriptableObject : public Object<ScriptableObject> {
    public:
        bool hasProperty(NPIdentifier propertyName)
        {
            return identifierIs(propertyName, "isMuted")
            || identifierIs(propertyName, "cachedIsMuted");
        }

        bool getProperty(NPIdentifier propertyName, NPVariant* result)
        {
            if (identifierIs(propertyName, "isMuted")) {
                BOOLEAN_TO_NPVARIANT(pluginTest()->isMuted(), *result);
                return true;
            }
            if (identifierIs(propertyName, "cachedIsMuted")) {
                BOOLEAN_TO_NPVARIANT(pluginTest()->cachedIsMuted(), *result);
                return true;
            }
            return false;
        }

    private:
        SetMuted* pluginTest() const { return static_cast<SetMuted*>(Object<ScriptableObject>::pluginTest()); }
    };

    virtual NPError NPP_New(NPMIMEType pluginType, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData *saved)
    {
        m_cachedIsMuted = isMuted();
        return NPERR_NO_ERROR;
    }

    virtual NPError NPP_GetValue(NPPVariable variable, void* value)
    {
        if (variable != NPPVpluginScriptableNPObject)
            return NPERR_GENERIC_ERROR;

        *(NPObject**)value = ScriptableObject::create(this);

        return NPERR_NO_ERROR;
    }

    virtual NPError NPP_SetValue(NPNVariable variable, void* value)
    {
        switch (variable) {
        case NPNVmuteAudioBool:
            m_cachedIsMuted = *(NPBool*)value;
            return NPERR_NO_ERROR;
        default:
            return NPERR_GENERIC_ERROR;
        }

    }
    bool m_cachedIsMuted;
};

static PluginTest::Register<SetMuted> setMuted("set-muted");
