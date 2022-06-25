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
#include "WebMockContentFilterManager.h"

#if ENABLE(CONTENT_FILTERING_IN_NETWORKING_PROCESS)

#include "NetworkConnectionToWebProcessMessages.h"
#include "NetworkProcessConnection.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/MockContentFilterManager.h>
#include <WebCore/MockContentFilterSettings.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

WebMockContentFilterManager& WebMockContentFilterManager::singleton()
{
    static NeverDestroyed<WebMockContentFilterManager> manager;
    return manager.get();
}

void WebMockContentFilterManager::startObservingSettings()
{
    WebCore::MockContentFilterManager::singleton().setClient(this);
}

void WebMockContentFilterManager::mockContentFilterSettingsChanged(WebCore::MockContentFilterSettings& settings)
{
    if (auto connection = WebProcess::singleton().existingNetworkProcessConnection())
        connection->connection().send(Messages::NetworkConnectionToWebProcess::InstallMockContentFilter(settings), 0);
}

};

#endif
