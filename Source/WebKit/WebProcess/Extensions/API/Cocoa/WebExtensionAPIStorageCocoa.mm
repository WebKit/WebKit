/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAPIStorage.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WebExtensionAPIStorageArea.h"

namespace WebKit {

WebExtensionAPIStorageArea& WebExtensionAPIStorage::local()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/local

    if (!m_local)
        m_local = WebExtensionAPIStorageArea::create(forMainWorld(), runtime(), extensionContext(), StorageType::Local);
    return *m_local;
}

WebExtensionAPIStorageArea& WebExtensionAPIStorage::session()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/session

    if (!m_session)
        m_session = WebExtensionAPIStorageArea::create(forMainWorld(), runtime(), extensionContext(), StorageType::Session);
    return *m_session;
}

WebExtensionAPIStorageArea& WebExtensionAPIStorage::sync()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/sync

    if (!m_sync)
        m_sync = WebExtensionAPIStorageArea::create(forMainWorld(), runtime(), extensionContext(), StorageType::Sync);
    return *m_sync;
}

WebExtensionAPIEvent& WebExtensionAPIStorage::onChanged()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/storage/onChanged

    if (!m_onChanged)
        m_onChanged = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::StorageOnChanged);
    return *m_onChanged;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
