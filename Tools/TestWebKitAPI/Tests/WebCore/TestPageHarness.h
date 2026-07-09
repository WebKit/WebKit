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

#pragma once

#include "TestPlatformStrategies.h"
#include <JavaScriptCore/InitializeThreading.h>
#include <WebCore/Document.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/Page.h>
#include <WebCoreTestSupport/TestPage.h>
#include <wtf/MainThread.h>
#include <wtf/text/WTFString.h>

namespace TestWebKitAPI {

class TestPageHarness {
public:
    static TestPageHarness create(WebCore::TestPageOptions options = { })
    {
        JSC::initialize();
        WTF::initializeMainThread();
        ensureTestPlatformStrategiesInstalled();
        return TestPageHarness(WebCore::createTestPage(options));
    }

    WebCore::Page& page() { return m_page.get(); }
    WebCore::LocalFrame& frame() { return *m_page->localMainFrame(); }
    WebCore::Document& document() { return *m_page->localTopDocument(); }

    void loadHTML(const String& html) { WebCore::loadHTMLIntoTestPage(frame(), html); }

private:
    explicit TestPageHarness(Ref<WebCore::Page>&& page)
        : m_page(WTF::move(page))
    {
    }

    Ref<WebCore::Page> m_page;
};

} // namespace TestWebKitAPI
