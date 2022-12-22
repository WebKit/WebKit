/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "WebDriverService.h"

#include "Capabilities.h"
#include "CapabilitiesSocket.h"

#include <dlfcn.h>

namespace WebDriver {

void WebDriverService::platformInit()
{
    dlopen("PosixWebKit", RTLD_NOW);

#if defined(ENABLE_STATIC_JSC) && !ENABLE_STATIC_JSC
    dlopen("libJavaScriptCore", RTLD_NOW);
#endif
}

Capabilities WebDriverService::platformCapabilities()
{
    Capabilities capabilities;
    capabilities.platformName = String::fromLatin1("playstation");
    capabilities.setWindowRect = true;
    return capabilities;
}

bool WebDriverService::platformCompareBrowserVersions(const String&, const String&)
{
    return true;
}

bool WebDriverService::platformValidateCapability(const String&, const Ref<JSON::Value>&) const
{
    return true;
}

bool WebDriverService::platformMatchCapability(const String&, const Ref<JSON::Value>&) const
{
    return true;
}

void WebDriverService::platformParseCapabilities(const JSON::Object& matchedCapabilities, Capabilities& capabilities) const
{
    CapabilitiesSocket::parseCapabilities(matchedCapabilities, capabilities);
}

bool WebDriverService::platformSupportProxyType(const String&) const
{
    return false;
}

} // namespace WebDriver
