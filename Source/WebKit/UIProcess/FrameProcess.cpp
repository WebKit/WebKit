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

#include "config.h"
#include "FrameProcess.h"

#include "BrowsingContextGroup.h"
#include "LoadedWebArchive.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessProxy.h"
#include <WebCore/Site.h>

namespace WebKit {

FrameProcess::FrameProcess(WebProcessProxy& process, BrowsingContextGroup& group, const std::optional<WebCore::Site>& site, const WebCore::Site& mainFrameSite,
    const WebPreferences& preferences, LoadedWebArchive loadedWebArchive, InjectBrowsingContextIntoProcess injectBrowsingContextIntoProcess)
    : m_process(process)
    , m_browsingContextGroup(group)
    , m_site(site)
    , m_mainFrameSite(mainFrameSite)
    , m_isArchiveProcess(loadedWebArchive == LoadedWebArchive::Yes)
{
    if (!preferences.siteIsolationEnabled()) {
        m_browsingContextGroup = nullptr;
        return;
    }

    if (injectBrowsingContextIntoProcess == InjectBrowsingContextIntoProcess::Yes)
        group.addFrameProcess(*this);

    if (!m_isArchiveProcess)
        process.didStartUsingProcessForSiteIsolation(site, mainFrameSite);

    ASSERT(isSharedProcess() == process.isSharedProcess());
}

FrameProcess::~FrameProcess()
{
    if (RefPtr group = m_browsingContextGroup.get())
        group->removeFrameProcess(*this);
}

}
