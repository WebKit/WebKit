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

#include <string.h>

using namespace std;

class URLRedirect : public PluginTest {
public:
    URLRedirect(NPP npp, const string& identifier)
        : PluginTest(npp, identifier)
    {
    }

    struct Redirect {
        int redirectsRemaining;
        bool async;
        bool hasFired;
    };

    std::map<void*, Redirect> redirects;

private:
    // This is the test object.
    class TestObject : public Object<TestObject> { };

    // This is the scriptable object. It has a single "testObject" property and an "evaluate" function.
    class ScriptableObject : public Object<ScriptableObject> {
    public:
        bool hasMethod(NPIdentifier methodName)
        {
            return identifierIs(methodName, "get") || identifierIs(methodName, "getAsync") || identifierIs(methodName, "serviceAsync");
        }

        bool get(const NPVariant* args, uint32_t argCount, NPVariant* result, bool async)
        {
            if (argCount != 3 || !NPVARIANT_IS_STRING(args[0]) || !(NPVARIANT_IS_BOOLEAN(args[1]) || NPVARIANT_IS_DOUBLE(args[1]) || NPVARIANT_IS_INT32(args[1])) || !NPVARIANT_IS_STRING(args[2]))
                return false;

            const NPString* notifyString = &NPVARIANT_TO_STRING(args[2]);
            basic_string<NPUTF8> notify(notifyString->UTF8Characters, notifyString->UTF8Length);
            NPIdentifier notifyMethod = pluginTest()->NPN_GetStringIdentifier(notify.c_str());

            Redirect& redirect = static_cast<URLRedirect*>(pluginTest())->redirects[reinterpret_cast<void*>(notifyMethod)];
            if (NPVARIANT_IS_DOUBLE(args[1]))
                redirect.redirectsRemaining = NPVARIANT_TO_DOUBLE(args[1]);
            else if (NPVARIANT_IS_INT32(args[1]))
                redirect.redirectsRemaining = NPVARIANT_TO_INT32(args[1]);
            else if (NPVARIANT_IS_BOOLEAN(args[1]))
                redirect.redirectsRemaining = NPVARIANT_TO_BOOLEAN(args[1]);
            redirect.async = async;
            redirect.hasFired = true;

            const NPString* urlString = &NPVARIANT_TO_STRING(args[0]);
            basic_string<NPUTF8> url(urlString->UTF8Characters, urlString->UTF8Length);

            pluginTest()->NPN_GetURLNotify(url.c_str(), 0, reinterpret_cast<void*>(notifyMethod));

            VOID_TO_NPVARIANT(*result);
            return true;
        }

        bool serviceAsync(const NPVariant* args, uint32_t argCount, NPVariant* result)
        {
            if (argCount)
                return false;

            NPBool seen = 0;
            URLRedirect* plugin = static_cast<URLRedirect*>(pluginTest());
            for (auto& redirect : plugin->redirects) {
                if (redirect.second.hasFired)
                    continue;
                redirect.second.hasFired = true;
                plugin->NPN_URLRedirectResponse(redirect.first, redirect.second.redirectsRemaining);
                if (redirect.second.redirectsRemaining)
                    --redirect.second.redirectsRemaining;
                seen = 1;
            }

            BOOLEAN_TO_NPVARIANT(seen, *result);
            return true;
        }

        bool invoke(NPIdentifier methodName, const NPVariant* args, uint32_t argCount, NPVariant* result)
        {
            if (identifierIs(methodName, "get"))
                return get(args, argCount, result, false);

            if (identifierIs(methodName, "getAsync"))
                return get(args, argCount, result, true);

            if (identifierIs(methodName, "serviceAsync"))
                return serviceAsync(args, argCount, result);

            return false;
        }
    };

    virtual NPError NPP_GetValue(NPPVariable variable, void *value)
    {
        if (variable != NPPVpluginScriptableNPObject)
            return NPERR_GENERIC_ERROR;

        *(NPObject**)value = ScriptableObject::create(this);

        return NPERR_NO_ERROR;
    }

    virtual bool NPP_URLNotify(const char* url, NPReason reason, void* notifyData)
    {
        NPVariant args[2];

        NPObject* windowScriptObject;
        NPN_GetValue(NPNVWindowNPObject, &windowScriptObject);

        NPIdentifier callbackIdentifier = notifyData;

        INT32_TO_NPVARIANT(reason, args[0]);
        STRINGZ_TO_NPVARIANT(url, args[1]);

        NPVariant browserResult;
        if (NPN_Invoke(windowScriptObject, callbackIdentifier, args, 2, &browserResult))
            NPN_ReleaseVariantValue(&browserResult);

        return true;
    }

    virtual void NPP_URLRedirectNotify(const char*, int32_t, void* notifyData)
    {
        Redirect& redirect = redirects[notifyData];
        if (redirect.async) {
            redirect.hasFired = false;
            return;
        }

        NPN_URLRedirectResponse(notifyData, redirect.redirectsRemaining);
        if (redirect.redirectsRemaining)
            --redirect.redirectsRemaining;
    }
};

static PluginTest::Register<URLRedirect> urlRedirect("url-redirect");

