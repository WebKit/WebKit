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
#include "PluginInfoProvider.h"

#include "Frame.h"
#include "FrameLoader.h"
#include "Page.h"
#include "SubframeLoader.h"

namespace WebCore {

PluginInfoProvider::~PluginInfoProvider()
{
    ASSERT(m_pages.computesEmpty());
}

void PluginInfoProvider::clearPagesPluginData()
{
    for (auto& page : m_pages)
        page.clearPluginData();
}

void PluginInfoProvider::refresh(bool reloadPages)
{
    refreshPlugins();

    Vector<Ref<Frame>> framesNeedingReload;

    for (auto& page : m_pages) {
        page.clearPluginData();

        if (!reloadPages)
            continue;

        for (AbstractFrame* frame = &page.mainFrame(); frame; frame = frame->tree().traverseNext()) {
            auto* localFrame = dynamicDowncast<LocalFrame>(frame);
            if (!localFrame)
                continue;
            if (localFrame->loader().subframeLoader().containsPlugins())
                framesNeedingReload.append(page.mainFrame());
        }
    }

    for (auto& frame : framesNeedingReload)
        frame->loader().reload();
}

void PluginInfoProvider::addPage(Page& page)
{
    ASSERT(!m_pages.contains(page));

    m_pages.add(page);
}

void PluginInfoProvider::removePage(Page& page)
{
    ASSERT(m_pages.contains(page));

    m_pages.remove(page);
}

}
