/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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

#include "SandboxExtension.h"
#include <WebCore/NetworkStorageSession.h>
#include <pal/SessionID.h>
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>

namespace WebKit {

struct WebProcessDataStoreParameters {
    using TopFrameDomain = WebCore::RegistrableDomain;
    using SubResourceDomain = WebCore::RegistrableDomain;

    PAL::SessionID sessionID;
    String mediaCacheDirectory;
#if !ENABLE(GPU_PROCESS)
    SandboxExtension::Handle mediaCacheDirectoryExtensionHandle;
#endif
    String mediaKeyStorageDirectory;
    SandboxExtension::Handle mediaKeyStorageDirectoryExtensionHandle;
    FileSystem::Salt mediaKeysStorageSalt;
    String javaScriptConfigurationDirectory;
    SandboxExtension::Handle javaScriptConfigurationDirectoryExtensionHandle;
    WebCore::ThirdPartyCookieBlockingMode thirdPartyCookieBlockingMode { WebCore::ThirdPartyCookieBlockingMode::All };
    HashSet<WebCore::RegistrableDomain> domainsWithUserInteraction;
    HashMap<TopFrameDomain, Vector<SubResourceDomain>> domainsWithStorageAccessQuirk;
#if ENABLE(ARKIT_INLINE_PREVIEW)
    String modelElementCacheDirectory;
    SandboxExtension::Handle modelElementCacheDirectoryExtensionHandle;
#endif
#if PLATFORM(IOS_FAMILY)
    std::optional<SandboxExtension::Handle> cookieStorageDirectoryExtensionHandle;
    std::optional<SandboxExtension::Handle> containerCachesDirectoryExtensionHandle;
    std::optional<SandboxExtension::Handle> containerTemporaryDirectoryExtensionHandle;
#endif
    bool trackingPreventionEnabled { false };
};

} // namespace WebKit
