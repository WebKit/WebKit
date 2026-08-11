/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "TestPage.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "DocumentWriter.h"
#include "EmptyClients.h"
#include "FrameLoader.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "Page.h"
#include "PageConfiguration.h"
#include "ProcessWarming.h"
#include "Settings.h"
#include <pal/SessionID.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

Ref<Page> createTestPage(const TestPageOptions& options)
{
    static bool prewarmed = [] {
        ProcessWarming::prewarmGlobally();
        return true;
    }();
    UNUSED_VARIABLE(prewarmed);

    Ref page = Page::create(pageConfigurationWithEmptyClients(std::nullopt, PAL::SessionID::defaultSessionID()));

    page->settings().setScriptEnabled(false);
    page->settings().setAcceleratedCompositingEnabled(false);
#if ENABLE(VIDEO)
    page->settings().setMediaEnabled(false);
#endif
    if (options.configureSettings)
        options.configureSettings(page->settings());

    RefPtr frame = page->localMainFrame();
    RELEASE_ASSERT(frame);

    frame->setView(LocalFrameView::create(*frame, options.viewportSize));
    frame->init();

    return page;
}

void loadHTMLIntoTestPage(LocalFrame& frame, const String& html)
{
    RefPtr activeDocumentLoader = frame.loader().activeDocumentLoader();
    ASSERT(activeDocumentLoader);
    auto& writer = activeDocumentLoader->writer();
    writer.setMIMEType("text/html"_s);
    writer.begin();
    writer.insertDataSynchronously(html);
    writer.end();

    if (RefPtr document = frame.document())
        document->updateLayoutIgnorePendingStylesheets();
}

} // namespace WebCore
