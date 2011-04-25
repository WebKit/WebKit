/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "WebInspector.h"

#if ENABLE(INSPECTOR)

#include "WebInspectorProxyMessages.h"
#include "WebPage.h"
#include "WebPageCreationParameters.h"
#include "WebProcess.h"
#include <WebCore/InspectorController.h>
#include <WebCore/Page.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebInspector> WebInspector::create(WebPage* page)
{
    return adoptRef(new WebInspector(page));
}

WebInspector::WebInspector(WebPage* page)
    : m_page(page)
    , m_inspectorPage(0)
{
}

// Called from WebInspectorClient
WebPage* WebInspector::createInspectorPage()
{
    if (!m_page)
        return 0;

    uint64_t inspectorPageID = 0;
    WebPageCreationParameters parameters;

    if (!WebProcess::shared().connection()->sendSync(Messages::WebInspectorProxy::CreateInspectorPage(),
            Messages::WebInspectorProxy::CreateInspectorPage::Reply(inspectorPageID, parameters),
            m_page->pageID(), CoreIPC::Connection::NoTimeout)) {
        return 0;
    }

    if (!inspectorPageID)
        return 0;

    WebProcess::shared().createWebPage(inspectorPageID, parameters);
    m_inspectorPage = WebProcess::shared().webPage(inspectorPageID);
    ASSERT(m_inspectorPage);

    return m_inspectorPage;
}

// Called from WebInspectorFrontendClient
void WebInspector::didLoadInspectorPage()
{
    WebProcess::shared().connection()->send(Messages::WebInspectorProxy::DidLoadInspectorPage(), m_page->pageID());
}

void WebInspector::didClose()
{
    WebProcess::shared().connection()->send(Messages::WebInspectorProxy::DidClose(), m_page->pageID());
}

void WebInspector::bringToFront()
{
    WebProcess::shared().connection()->send(Messages::WebInspectorProxy::BringToFront(), m_page->pageID());
}

void WebInspector::inspectedURLChanged(const String& urlString)
{
    WebProcess::shared().connection()->send(Messages::WebInspectorProxy::InspectedURLChanged(urlString), m_page->pageID());
}

void WebInspector::attach()
{
    WebProcess::shared().connection()->send(Messages::WebInspectorProxy::Attach(), m_page->pageID());
}

void WebInspector::detach()
{
    WebProcess::shared().connection()->send(Messages::WebInspectorProxy::Detach(), m_page->pageID());
}

void WebInspector::setAttachedWindowHeight(unsigned height)
{
    WebProcess::shared().connection()->send(Messages::WebInspectorProxy::SetAttachedWindowHeight(height), m_page->pageID());
}

// Called by WebInspector messages
void WebInspector::show()
{
    m_page->corePage()->inspectorController()->show();
}

void WebInspector::close()
{
    m_page->corePage()->inspectorController()->close();
}

void WebInspector::evaluateScriptForTest(long callID, const String& script)
{
    m_page->corePage()->inspectorController()->evaluateForTestInFrontend(callID, script);
}

void WebInspector::showConsole()
{
    m_page->corePage()->inspectorController()->showConsole();
}

void WebInspector::startJavaScriptDebugging()
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_page->corePage()->inspectorController()->showAndEnableDebugger();
#endif
}

void WebInspector::stopJavaScriptDebugging()
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_page->corePage()->inspectorController()->disableDebugger();
#endif
}

void WebInspector::startJavaScriptProfiling()
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_page->corePage()->inspectorController()->startUserInitiatedProfiling();
#endif
}

void WebInspector::stopJavaScriptProfiling()
{
#if ENABLE(JAVASCRIPT_DEBUGGER)
    m_page->corePage()->inspectorController()->stopUserInitiatedProfiling();
#endif
}

void WebInspector::startPageProfiling()
{
    m_page->corePage()->inspectorController()->startTimelineProfiler();
}

void WebInspector::stopPageProfiling()
{
    m_page->corePage()->inspectorController()->stopTimelineProfiler();
    // FIXME: show the Timeline panel.
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR)
