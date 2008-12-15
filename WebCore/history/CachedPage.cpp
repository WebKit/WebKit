/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CachedPage.h"

#include "AnimationController.h"
#include "CachedPagePlatformData.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "GCController.h"
#include "JSDOMWindow.h"
#include "JSDOMWindowShell.h"
#include "Logging.h"
#include "Page.h"
#include "PageGroup.h"
#include "SystemTime.h"
#include "ScriptController.h"
#include <runtime/JSLock.h>
#include <wtf/RefCountedLeakCounter.h>

#if ENABLE(SVG)
#include "SVGDocumentExtensions.h"
#endif

using namespace JSC;

namespace WebCore {

#ifndef NDEBUG
static WTF::RefCountedLeakCounter cachedPageCounter("CachedPage");
#endif

PassRefPtr<CachedPage> CachedPage::create(Page* page)
{
    return adoptRef(new CachedPage(page));
}

CachedPage::CachedPage(Page* page)
    : m_timeStamp(0)
    , m_document(page->mainFrame()->document())
    , m_view(page->mainFrame()->view())
    , m_mousePressNode(page->mainFrame()->eventHandler()->mousePressNode())
    , m_URL(page->mainFrame()->loader()->url())
{
#ifndef NDEBUG
    cachedPageCounter.increment();
#endif
    
    m_document->documentWillBecomeInactive(); 
    
    Frame* mainFrame = page->mainFrame();
    mainFrame->clearTimers();

    JSLock lock(false);

    ScriptController* proxy = mainFrame->script();
    if (proxy->haveWindowShell()) {
        m_window = proxy->windowShell()->window();
    }

    m_document->setInPageCache(true);
}

CachedPage::~CachedPage()
{
#ifndef NDEBUG
    cachedPageCounter.decrement();
#endif

    clear();
}

DOMWindow* CachedPage::domWindow() const
{
    return m_window ? m_window->impl() : 0;
}

void CachedPage::restore(Page* page)
{
    ASSERT(m_document->view() == m_view);

    Frame* mainFrame = page->mainFrame();

    JSLock lock(false);

    ScriptController* proxy = mainFrame->script();
    if (proxy->haveWindowShell()) {
        JSDOMWindowShell* windowShell = proxy->windowShell();
        if (m_window) {
            windowShell->setWindow(m_window.get());
        } else {
            windowShell->setWindow(mainFrame->domWindow());
            proxy->attachDebugger(page->debugger());
            windowShell->window()->setProfileGroup(page->group().identifier());
        }
    }

#if ENABLE(SVG)
    if (m_document && m_document->svgExtensions())
        m_document->accessSVGExtensions()->unpauseAnimations();
#endif

    mainFrame->animation()->resumeAnimations(m_document.get());

    mainFrame->eventHandler()->setMousePressNode(mousePressNode());
        
    // Restore the focus appearance for the focused element.
    // FIXME: Right now we don't support pages w/ frames in the b/f cache.  This may need to be tweaked when we add support for that.
    Document* focusedDocument = page->focusController()->focusedOrMainFrame()->document();
    if (Node* node = focusedDocument->focusedNode()) {
        if (node->isElementNode())
            static_cast<Element*>(node)->updateFocusAppearance(true);
    }
}

void CachedPage::clear()
{
    if (!m_document)
        return;

    if (m_cachedPagePlatformData)
        m_cachedPagePlatformData->clear();
        
    ASSERT(m_view);
    ASSERT(m_document->frame() == m_view->frame());

    if (m_document->inPageCache()) {
        Frame::clearTimers(m_view.get(), m_document.get());

        m_document->setInPageCache(false);
        // FIXME: We don't call willRemove here. Why is that OK?
        m_document->detach();
        m_document->removeAllEventListenersFromAllNodes();

        m_view->clearFrame();
    }

    ASSERT(!m_document->inPageCache());

    m_document = 0;
    m_view = 0;
    m_mousePressNode = 0;
    m_URL = KURL();

    JSLock lock(false);
    m_window = 0;

    m_cachedPagePlatformData.clear();

    gcController().garbageCollectSoon();
}

void CachedPage::setDocumentLoader(PassRefPtr<DocumentLoader> loader)
{
    m_documentLoader = loader;
}

DocumentLoader* CachedPage::documentLoader()
{
    return m_documentLoader.get();
}

void CachedPage::setTimeStamp(double timeStamp)
{
    m_timeStamp = timeStamp;
}

void CachedPage::setTimeStampToNow()
{
    m_timeStamp = currentTime();
}

double CachedPage::timeStamp() const
{
    return m_timeStamp;
}

void CachedPage::setCachedPagePlatformData(CachedPagePlatformData* data)
{
    m_cachedPagePlatformData.set(data);
}

CachedPagePlatformData* CachedPage::cachedPagePlatformData()
{
    return m_cachedPagePlatformData.get();
}

} // namespace WebCore
