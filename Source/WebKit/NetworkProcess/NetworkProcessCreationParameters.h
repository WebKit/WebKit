/*
 * Copyright (C) 2012-2017 Apple Inc. All rights reserved.
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

#include "AuxiliaryProcessCreationParameters.h"
#include "CacheModel.h"
#include "SandboxExtension.h"
#include <WebCore/Cookie.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/RegistrableDomain.h>
#include <wtf/ProcessID.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

#if USE(SOUP)
#include <WebCore/HTTPCookieAcceptPolicy.h>
#include <wtf/MemoryPressureHandler.h>
#endif

namespace WebKit {

struct WebsiteDataStoreParameters;

struct NetworkProcessCreationParameters {
    AuxiliaryProcessCreationParameters auxiliaryProcessParameters;

    CacheModel cacheModel { CacheModel::DocumentViewer };

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    Vector<uint8_t> uiProcessCookieStorageIdentifier;
#endif
    bool shouldSuppressMemoryPressureHandler { false };

    Vector<String> urlSchemesRegisteredForCustomProtocols;

#if PLATFORM(COCOA)
    String uiProcessBundleIdentifier;
    RetainPtr<CFDataRef> networkATSContext;
#endif

#if USE(SOUP)
    WebCore::HTTPCookieAcceptPolicy cookieAcceptPolicy { WebCore::HTTPCookieAcceptPolicy::AlwaysAccept };
    Vector<String> languages;
    std::optional<MemoryPressureHandler::Configuration> memoryPressureHandlerConfiguration;
#endif

    Vector<String> urlSchemesRegisteredAsSecure;
    Vector<String> urlSchemesRegisteredAsBypassingContentSecurityPolicy;
    Vector<String> urlSchemesRegisteredAsLocal;
    Vector<String> urlSchemesRegisteredAsNoAccess;

    bool enablePrivateClickMeasurement { true };
    bool ftpEnabled { false };

    Vector<WebsiteDataStoreParameters> websiteDataStoreParameters;
    Vector<std::pair<WebCore::ProcessIdentifier, WebCore::RegistrableDomain>> allowedFirstPartiesForCookies;
    HashSet<String> localhostAliasesForTesting;
};

} // namespace WebKit
