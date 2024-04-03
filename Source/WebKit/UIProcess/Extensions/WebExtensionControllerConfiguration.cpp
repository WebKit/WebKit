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

#include "config.h"
#include "WebExtensionControllerConfiguration.h"

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

WebExtensionControllerConfiguration::WebExtensionControllerConfiguration(IsPersistent persistent)
    : m_storageDirectory(persistent == IsPersistent::Yes ? createStorageDirectoryPath() : nullString())
{
}

WebExtensionControllerConfiguration::WebExtensionControllerConfiguration(TemporaryTag, const String& storageDirectory)
    : m_temporary(true)
    , m_storageDirectory(!storageDirectory.isEmpty() ? storageDirectory : createTemporaryStorageDirectoryPath())
{
}

WebExtensionControllerConfiguration::WebExtensionControllerConfiguration(const WTF::UUID& identifier)
    : m_identifier(identifier)
    , m_storageDirectory(createStorageDirectoryPath(identifier))
{
}

bool WebExtensionControllerConfiguration::operator==(const WebExtensionControllerConfiguration& other) const
{
    return this == &other || (m_identifier == other.m_identifier && m_storageDirectory == other.m_storageDirectory && m_webViewConfiguration == other.m_webViewConfiguration && m_defaultWebsiteDataStore == other.m_defaultWebsiteDataStore);
}

WebsiteDataStore& WebExtensionControllerConfiguration::defaultWebsiteDataStore() const
{
    if (m_defaultWebsiteDataStore)
        return *m_defaultWebsiteDataStore;
    return WebsiteDataStore::defaultDataStore();
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
