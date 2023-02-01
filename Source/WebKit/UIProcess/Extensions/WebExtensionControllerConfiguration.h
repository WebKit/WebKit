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

#include "APIObject.h"
#include <wtf/Forward.h>
#include <wtf/Markable.h>
#include <wtf/UUID.h>

OBJC_CLASS _WKWebExtensionControllerConfiguration;

namespace WebKit {

class WebExtensionControllerConfiguration : public API::ObjectImpl<API::Object::Type::WebExtensionControllerConfiguration> {
    WTF_MAKE_NONCOPYABLE(WebExtensionControllerConfiguration);

public:
    enum class IsPersistent : bool { No, Yes };

    static Ref<WebExtensionControllerConfiguration> createDefault() { return adoptRef(*new WebExtensionControllerConfiguration(IsPersistent::Yes)); }
    static Ref<WebExtensionControllerConfiguration> createNonPersistent() { return adoptRef(*new WebExtensionControllerConfiguration(IsPersistent::No)); }
    static Ref<WebExtensionControllerConfiguration> create(const UUID& identifier) { return adoptRef(*new WebExtensionControllerConfiguration(identifier)); }

    Ref<WebExtensionControllerConfiguration> copy() const;

    explicit WebExtensionControllerConfiguration(IsPersistent);
    explicit WebExtensionControllerConfiguration(const UUID&);

    std::optional<UUID> identifier() const { return m_identifier; }

    bool storageIsPersistent() const { return !m_storageDirectory.isEmpty(); }
    String storageDirectory() const { return m_storageDirectory; }

    bool operator==(const WebExtensionControllerConfiguration&) const;
    bool operator!=(const WebExtensionControllerConfiguration& other) const { return !(this == &other); }

#ifdef __OBJC__
    _WKWebExtensionControllerConfiguration *wrapper() const { return (_WKWebExtensionControllerConfiguration *)API::ObjectImpl<API::Object::Type::WebExtensionControllerConfiguration>::wrapper(); }
#endif

private:
    static String createStorageDirectoryPath(std::optional<UUID> = std::nullopt);

    Markable<UUID> m_identifier;
    String m_storageDirectory;
};

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
