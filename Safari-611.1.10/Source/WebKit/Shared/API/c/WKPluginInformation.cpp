/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "WKPluginInformation.h"

#include "APIString.h"
#include "PluginInformation.h"
#include "WKSharedAPICast.h"

WKStringRef WKPluginInformationBundleIdentifierKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::pluginInformationBundleIdentifierKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}

WKStringRef WKPluginInformationBundleVersionKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::pluginInformationBundleVersionKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}

WKStringRef WKPluginInformationBundleShortVersionKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::pluginInformationBundleShortVersionKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}

WKStringRef WKPluginInformationPathKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::pluginInformationPathKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}

WKStringRef WKPluginInformationDisplayNameKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::pluginInformationDisplayNameKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}

WKStringRef WKPluginInformationDefaultLoadPolicyKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::pluginInformationDefaultLoadPolicyKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}

WKStringRef WKPluginInformationUpdatePastLastBlockedVersionIsKnownAvailableKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::pluginInformationUpdatePastLastBlockedVersionIsKnownAvailableKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}

WKStringRef WKPluginInformationHasSandboxProfileKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::pluginInformationHasSandboxProfileKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}

WKStringRef WKPluginInformationFrameURLKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::pluginInformationFrameURLKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}

WKStringRef WKPluginInformationMIMETypeKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::pluginInformationMIMETypeKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}

WKStringRef WKPluginInformationPageURLKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::pluginInformationPageURLKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}

WKStringRef WKPluginInformationPluginspageAttributeURLKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::pluginInformationPluginspageAttributeURLKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}

WKStringRef WKPluginInformationPluginURLKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::pluginInformationPluginURLKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}

WKStringRef WKPlugInInformationReplacementObscuredKey()
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    static API::String& key = API::String::create(WebKit::plugInInformationReplacementObscuredKey()).leakRef();
    return WebKit::toAPI(&key);
#else
    return 0;
#endif
}
