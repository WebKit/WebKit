/*
 * Copyright (C) 2017 Igalia S.L.
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

#pragma once

#include <wtf/EnumTraits.h>
#include <wtf/HashMap.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

struct SoupNetworkProxySettings {
    enum class Mode { Default, NoProxy, Custom };

    SoupNetworkProxySettings() = default;

    explicit SoupNetworkProxySettings(Mode proxyMode)
        : mode(proxyMode)
    {
    }

    SoupNetworkProxySettings(const WebCore::SoupNetworkProxySettings& other)
        : mode(other.mode)
        , defaultProxyURL(other.defaultProxyURL)
        , ignoreHosts(g_strdupv(other.ignoreHosts.get()))
        , proxyMap(other.proxyMap)
    {
    }

    SoupNetworkProxySettings& operator=(const WebCore::SoupNetworkProxySettings& other)
    {
        mode = other.mode;
        defaultProxyURL = other.defaultProxyURL;
        ignoreHosts.reset(g_strdupv(other.ignoreHosts.get()));
        proxyMap = other.proxyMap;
        return *this;
    }

    bool isEmpty() const { return mode == Mode::Custom && defaultProxyURL.isNull() && !ignoreHosts && proxyMap.isEmpty(); }

    Mode mode { Mode::Default };
    CString defaultProxyURL;
    GUniquePtr<char*> ignoreHosts;
    HashMap<CString, CString> proxyMap;
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::SoupNetworkProxySettings::Mode> {
    using values = EnumValues<
        WebCore::SoupNetworkProxySettings::Mode,
        WebCore::SoupNetworkProxySettings::Mode::Default,
        WebCore::SoupNetworkProxySettings::Mode::NoProxy,
        WebCore::SoupNetworkProxySettings::Mode::Custom
    >;
};

} // namespace WTF
