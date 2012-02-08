/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
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
#include "WebInspectorProxy.h"

#if ENABLE(INSPECTOR)

#include "WebInspectorMessages.h"
#include "WebPageCreationParameters.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessProxy.h"

#if PLATFORM(WIN)
#include "WebView.h"
#endif

using namespace WebCore;

namespace WebKit {

static PassRefPtr<WebPageGroup> createInspectorPageGroup()
{
    RefPtr<WebPageGroup> pageGroup = WebPageGroup::create("__WebInspectorPageGroup__", false, false);

#ifndef NDEBUG
    // Allow developers to inspect the Web Inspector in debug builds.
    pageGroup->preferences()->setDeveloperExtrasEnabled(true);
#endif

    pageGroup->preferences()->setApplicationChromeModeEnabled(true);
    pageGroup->preferences()->setSuppressIncrementalRendering(true);

    return pageGroup.release();
}

WebPageGroup* WebInspectorProxy::inspectorPageGroup()
{
    static WebPageGroup* pageGroup = createInspectorPageGroup().leakRef();
    return pageGroup;
}

WebInspectorProxy::WebInspectorProxy(WebPageProxy* page)
    : m_page(page)
    , m_isVisible(false)
    , m_isAttached(false)
    , m_isDebuggingJavaScript(false)
    , m_isProfilingJavaScript(false)
    , m_isProfilingPage(false)
#if PLATFORM(WIN)
    , m_inspectorWindow(0)
#elif PLATFORM(GTK)
    , m_inspectorView(0)
    , m_inspectorWindow(0)
#endif
{
}

WebInspectorProxy::~WebInspectorProxy()
{
}

void WebInspectorProxy::invalidate()
{
    m_page->close();
    didClose();

    m_page = 0;

    m_isVisible = false;
    m_isDebuggingJavaScript = false;
    m_isProfilingJavaScript = false;
    m_isProfilingPage = false;
}

// Public APIs
void WebInspectorProxy::show()
{
    if (!m_page)
        return;

    m_page->process()->send(Messages::WebInspector::Show(), m_page->pageID());
}

void WebInspectorProxy::close()
{
    if (!m_page)
        return;

    m_page->process()->send(Messages::WebInspector::Close(), m_page->pageID());
}

void WebInspectorProxy::showConsole()
{
    if (!m_page)
        return;

    m_page->process()->send(Messages::WebInspector::ShowConsole(), m_page->pageID());
}

void WebInspectorProxy::attach()
{
    if (!canAttach())
        return;

    m_isAttached = true;

    if (m_isVisible)
        inspectorPageGroup()->preferences()->setInspectorStartsAttached(true);

    platformAttach();
}

void WebInspectorProxy::detach()
{
    m_isAttached = false;
    
    if (m_isVisible)
        inspectorPageGroup()->preferences()->setInspectorStartsAttached(false);

    platformDetach();
}

void WebInspectorProxy::setAttachedWindowHeight(unsigned height)
{
    inspectorPageGroup()->preferences()->setInspectorAttachedHeight(height);
    platformSetAttachedWindowHeight(height);
}

void WebInspectorProxy::toggleJavaScriptDebugging()
{
    if (!m_page)
        return;

    if (m_isDebuggingJavaScript)
        m_page->process()->send(Messages::WebInspector::StopJavaScriptDebugging(), m_page->pageID());
    else
        m_page->process()->send(Messages::WebInspector::StartJavaScriptDebugging(), m_page->pageID());

    // FIXME: have the WebProcess notify us on state changes.
    m_isDebuggingJavaScript = !m_isDebuggingJavaScript;
}

void WebInspectorProxy::toggleJavaScriptProfiling()
{
    if (!m_page)
        return;

    if (m_isProfilingJavaScript)
        m_page->process()->send(Messages::WebInspector::StopJavaScriptProfiling(), m_page->pageID());
    else
        m_page->process()->send(Messages::WebInspector::StartJavaScriptProfiling(), m_page->pageID());

    // FIXME: have the WebProcess notify us on state changes.
    m_isProfilingJavaScript = !m_isProfilingJavaScript;
}

void WebInspectorProxy::togglePageProfiling()
{
    if (!m_page)
        return;

    if (m_isProfilingPage)
        m_page->process()->send(Messages::WebInspector::StopPageProfiling(), m_page->pageID());
    else
        m_page->process()->send(Messages::WebInspector::StartPageProfiling(), m_page->pageID());

    // FIXME: have the WebProcess notify us on state changes.
    m_isProfilingPage = !m_isProfilingPage;
}

bool WebInspectorProxy::isInspectorPage(WebPageProxy* page)
{
    return page->pageGroup() == inspectorPageGroup();
}

// Called by WebInspectorProxy messages
void WebInspectorProxy::createInspectorPage(uint64_t& inspectorPageID, WebPageCreationParameters& inspectorPageParameters)
{
    inspectorPageID = 0;

    if (!m_page)
        return;

    m_isAttached = shouldOpenAttached();

    WebPageProxy* inspectorPage = platformCreateInspectorPage();
    ASSERT(inspectorPage);
    if (!inspectorPage)
        return;

    inspectorPageID = inspectorPage->pageID();
    inspectorPageParameters = inspectorPage->creationParameters();

    String url = inspectorPageURL();
    if (m_isAttached)
        url += "?docked=true";
    m_page->process()->assumeReadAccessToBaseURL(inspectorBaseURL());
    inspectorPage->loadURL(url);
}

void WebInspectorProxy::didLoadInspectorPage()
{
    m_isVisible = true;

    // platformOpen is responsible for rendering attached mode depending on m_isAttached.
    platformOpen();
}

void WebInspectorProxy::didClose()
{
    m_isVisible = false;
    m_isDebuggingJavaScript = false;
    m_isProfilingJavaScript = false;
    m_isProfilingPage = false;

    if (m_isAttached) {
        // Detach here so we only need to have one code path that is responsible for cleaning up the inspector
        // state.
        detach();
    }

    platformDidClose();
}

void WebInspectorProxy::bringToFront()
{
    platformBringToFront();
}

void WebInspectorProxy::inspectedURLChanged(const String& urlString)
{
    platformInspectedURLChanged(urlString);
}

bool WebInspectorProxy::canAttach()
{
    unsigned inspectedWindowHeight = platformInspectedWindowHeight();
    return inspectedWindowHeight && minimumAttachedHeight <= (inspectedWindowHeight * 3 / 4);
}

bool WebInspectorProxy::shouldOpenAttached()
{
    return inspectorPageGroup()->preferences()->inspectorStartsAttached() && canAttach();
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR)
