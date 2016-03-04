/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WebResourceLoadStatisticsStore.h"

#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebResourceLoadStatisticsStoreMessages.h"
#include <WebCore/ResourceLoadStatisticsStore.h>

using namespace WebCore;

namespace WebKit {

Ref<WebResourceLoadStatisticsStore> WebResourceLoadStatisticsStore::create(const String& resourceLoadStatisticsDirectory)
{
    return adoptRef(*new WebResourceLoadStatisticsStore(resourceLoadStatisticsDirectory));
}

WebResourceLoadStatisticsStore::WebResourceLoadStatisticsStore(const String& resourceLoadStatisticsDirectory)
    : m_resourceStatisticsStore(WebCore::ResourceLoadStatisticsStore::create(resourceLoadStatisticsDirectory))
{
}

WebResourceLoadStatisticsStore::~WebResourceLoadStatisticsStore()
{
}

void WebResourceLoadStatisticsStore::resourceLoadStatisticsUpdated(const Vector<WebCore::ResourceLoadStatistics>& origins)
{
    coreStore().mergeStatistics(origins);
}

void WebResourceLoadStatisticsStore::setResourceLoadStatisticsEnabled(bool enabled)
{
    if (enabled == m_resourceLoadStatisticsEnabled)
        return;

    m_resourceLoadStatisticsEnabled = enabled;

    readDataFromDiskIfNeeded();
}

bool WebResourceLoadStatisticsStore::resourceLoadStatisticsEnabled() const
{
    return m_resourceLoadStatisticsEnabled;
}

void WebResourceLoadStatisticsStore::readDataFromDiskIfNeeded()
{
    if (!m_resourceLoadStatisticsEnabled)
        return;

    coreStore().readDataFromDiskIfNeeded();
}

void WebResourceLoadStatisticsStore::writeToDisk()
{
    if (!m_resourceLoadStatisticsEnabled)
        return;
    
    coreStore().writeDataToDisk();
}

void WebResourceLoadStatisticsStore::applicationWillTerminate()
{
    if (!m_resourceLoadStatisticsEnabled)
        return;

    // FIXME(154642): TEMPORARY CODE: This should not be done in one long operation when exiting.
    writeToDisk();
}

} // namespace WebKit
