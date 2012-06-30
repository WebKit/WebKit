/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

class LeakWindowScriptableObject : public PluginTest {
public:
    LeakWindowScriptableObject(NPP npp, const string& identifier)
        : PluginTest(npp, identifier)
    {
    }

private:
    virtual NPError NPP_New(NPMIMEType pluginType, uint16_t mode, int16_t argc, char *argn[], char *argv[], NPSavedData *saved)
    {
        // Get a new reference to the window script object.
        NPObject* window;
        if (NPN_GetValue(NPNVWindowNPObject, &window) != NPERR_NO_ERROR) {
            log("Fail: Cannot fetch window script object");
            return NPERR_NO_ERROR;
        }

        // Get another reference to the same object via window.self.
        NPIdentifier self_name = NPN_GetStringIdentifier("self");
        NPVariant window_self_variant;
        if (!NPN_GetProperty(window, self_name, &window_self_variant)) {
            log("Fail: Cannot query window.self");
            return NPERR_NO_ERROR;
        }
        if (!NPVARIANT_IS_OBJECT(window_self_variant)) {
            log("Fail: window.self is not an object");
            return NPERR_NO_ERROR;
        }

        // Leak both references to the window script object.
        return NPERR_NO_ERROR;
    }
};

static PluginTest::Register<LeakWindowScriptableObject> leakWindowScriptableObject("leak-window-scriptable-object");
