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

#include <utility>
#include <wtf/Forward.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebDriver {

struct Timeouts {
    std::optional<double> script;
    std::optional<double> pageLoad;
    std::optional<double> implicit;
};

enum class PageLoadStrategy {
    None,
    Normal,
    Eager
};

enum class UnhandledPromptBehavior {
    Dismiss,
    Accept,
    DismissAndNotify,
    AcceptAndNotify,
    Ignore
};

struct Proxy {
    String type;
    std::optional<URL> autoconfigURL;
    std::optional<URL> ftpURL;
    std::optional<URL> httpURL;
    std::optional<URL> httpsURL;
    std::optional<URL> socksURL;
    std::optional<uint8_t> socksVersion;
    Vector<String> ignoreAddressList;
};

struct Capabilities {
    std::optional<String> browserName;
    std::optional<String> browserVersion;
    std::optional<String> platformName;
    std::optional<bool> acceptInsecureCerts;
    std::optional<bool> strictFileInteractability;
    std::optional<bool> setWindowRect;
    std::optional<Timeouts> timeouts;
    std::optional<PageLoadStrategy> pageLoadStrategy;
    std::optional<UnhandledPromptBehavior> unhandledPromptBehavior;
    std::optional<Proxy> proxy;
    std::optional<String> targetAddr;
    std::optional<int> targetPort;
#if PLATFORM(GTK) || PLATFORM(WPE)
    std::optional<String> browserBinary;
    std::optional<Vector<String>> browserArguments;
    std::optional<Vector<std::pair<String, String>>> certificates;
#endif
#if PLATFORM(GTK)
    std::optional<bool> useOverlayScrollbars;
#endif
};

} // namespace WebDriver
