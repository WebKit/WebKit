/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

#include "APIData.h"
#include "WebExtensionContextIdentifier.h"
#include "WebExtensionStorageAccessLevel.h"
#include "WebExtensionTabIdentifier.h"
#include "WebExtensionWindowIdentifier.h"
#include <WebCore/PageIdentifier.h>
#include <pal/SessionID.h>
#include <tuple>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Markable.h>
#include <wtf/URL.h>
#include <wtf/Vector.h>
#include <wtf/WallTime.h>

namespace WebKit {

enum class WebExtensionContextInstallReason : uint8_t {
    None,
    ExtensionInstall,
    ExtensionUpdate,
    BrowserUpdate,
};

enum class WebExtensionContextWindowIsClosing : bool { No, Yes };

using WebExtensionPermissionsMap = HashMap<String, WallTime>;

using WebExtensionPageIdentifierTuple = std::tuple<WebCore::PageIdentifier, std::optional<WebExtensionTabIdentifier>, std::optional<WebExtensionWindowIdentifier>>;

struct WebExtensionContextParameters {
    WebExtensionContextIdentifier unprivilegedIdentifier;
    Markable<WebExtensionContextIdentifier> privilegedIdentifier;

    URL baseURL;
    String uniqueIdentifier;
    HashSet<String> unsupportedAPIs;

    WebExtensionPermissionsMap grantedPermissions;

    RefPtr<API::Data> localizationJSON;
    RefPtr<API::Data> manifestJSON;

    double manifestVersion { 0 };
    bool usesStructuredCloneMessageSerialization { false };

    WebExtensionStorageAccessLevelMap storageAccessLevels;

    PAL::SessionID defaultSessionID;

    std::optional<WebCore::PageIdentifier> backgroundPageIdentifier;
#if ENABLE(INSPECTOR_EXTENSIONS)
    Vector<WebExtensionPageIdentifierTuple> inspectorPageIdentifiers;
    Vector<WebExtensionPageIdentifierTuple> inspectorBackgroundPageIdentifiers;
#endif
    Vector<WebExtensionPageIdentifierTuple> popupPageIdentifiers;
    Vector<WebExtensionPageIdentifierTuple> tabPageIdentifiers;
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
