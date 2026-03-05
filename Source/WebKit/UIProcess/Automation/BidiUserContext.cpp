/*
 * Copyright (C) 2025 Microsoft Corporation. All rights reserved.
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
#include "BidiUserContext.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include <wtf/Vector.h>

namespace WebKit {

#if USE(GLIB)
BidiUserContext::BidiUserContext(WebsiteDataStore& dataStore, WebProcessPool& processPool, GRefPtr<WebKitWebContext>&& context)
    : m_dataStore(dataStore)
    , m_processPool(processPool)
    , m_context(WTF::move(context))
{
};
#else
BidiUserContext::BidiUserContext(WebsiteDataStore& dataStore, WebProcessPool& processPool)
    : m_dataStore(dataStore)
    , m_processPool(processPool)
{
};
#endif // PLATFORM(GTK)

BidiUserContext::~BidiUserContext() = default;

Vector<Ref<WebPageProxy>> BidiUserContext::allPages() const
{
    Vector<Ref<WebPageProxy>> pages;
    for (Ref process : m_processPool->processes()) {
        for (Ref page : process->pages()) {
            if (page->websiteDataStore() == m_dataStore.get())
                pages.append(WTF::move(page));
        }
    }
    return pages;
}

} // namespace WebKit

#endif // ENABLE(WEBDRIVER_BIDI)

