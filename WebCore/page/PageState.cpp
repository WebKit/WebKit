/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "PageState.h"

#include "Document.h"
#include "Element.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "Page.h"
#ifdef SVG_SUPPORT
#include "SVGDocumentExtensions.h"
#endif
#include "kjs_proxy.h"
#include "kjs_window.h"
#include "kjs_window.h"
#include <kjs/JSLock.h>
#include <kjs/SavedBuiltins.h>
#include <kjs/property_map.h>

using namespace KJS;

namespace WebCore {

PassRefPtr<PageState> PageState::create(Page* page)
{
    return new PageState(page);
}

PageState::PageState(Page* page)
    : m_document(page->mainFrame()->document())
    , m_view(page->mainFrame()->view())
    , m_mousePressNode(page->mainFrame()->eventHandler()->mousePressNode())
    , m_URL(page->mainFrame()->loader()->url())
    , m_windowProperties(new SavedProperties)
    , m_locationProperties(new SavedProperties)
    , m_interpreterBuiltins(new SavedBuiltins)
{
    Frame* mainFrame = page->mainFrame();
    KJSProxy* proxy = mainFrame->scriptProxy();
    Window* window = Window::retrieveWindow(mainFrame);

    mainFrame->clearTimers();

    JSLock lock;

    if (proxy && window) {
        proxy->interpreter()->saveBuiltins(*m_interpreterBuiltins.get());
        window->saveProperties(*m_windowProperties.get());
        window->location()->saveProperties(*m_locationProperties.get());
        m_pausedTimeouts.set(window->pauseTimeouts());
    }

    m_document->setInPageCache(true);

#ifdef SVG_SUPPORT
    if (m_document && m_document->svgExtensions())
        m_document->accessSVGExtensions()->pauseAnimations();
#endif
}

PageState::~PageState()
{
    clear();
}

void PageState::restore(Page* page)
{
    Frame* mainFrame = page->mainFrame();
    KJSProxy* proxy = mainFrame->scriptProxy();
    Window* window = Window::retrieveWindow(mainFrame);

    JSLock lock;

    if (proxy && window) {
        proxy->interpreter()->restoreBuiltins(*m_interpreterBuiltins.get());
        window->restoreProperties(*m_windowProperties.get());
        window->location()->restoreProperties(*m_locationProperties.get());
        window->resumeTimeouts(m_pausedTimeouts.get());
    }

#ifdef SVG_SUPPORT
    if (m_document && m_document->svgExtensions())
        m_document->accessSVGExtensions()->unpauseAnimations();
#endif

    mainFrame->eventHandler()->setMousePressNode(mousePressNode());
        
    // Restore the focus appearance for the focused element.
    // FIXME: Right now we don't support pages w/ frames in the b/f cache.  This may need to be tweaked when we add support for that.
    Document* focusedDocument = page->focusController()->focusedOrMainFrame()->document();
    if (Node* node = focusedDocument->focusedNode()) {
        if (node->isElementNode())
            static_cast<Element*>(node)->updateFocusAppearance(true);
    }
}

void PageState::clear()
{
    if (!m_document)
        return;

    ASSERT(m_view);
    ASSERT(m_document->view() == m_view);

    if (m_document->inPageCache()) {
        Frame::clearTimers(m_view.get());

        bool detached = !m_document->renderer();
        m_document->setInPageCache(false);
        if (detached) {
            m_document->detach();
            m_document->removeAllEventListenersFromAllNodes();
        }

        m_view->clearPart();
    }

    ASSERT(!m_document->inPageCache());

    m_document = 0;
    m_view = 0;
    m_mousePressNode = 0;
    m_URL = KURL();

    JSLock lock;

    m_windowProperties.clear();
    m_locationProperties.clear();
    m_interpreterBuiltins.clear();
    m_pausedTimeouts.clear();

    Collector::collect();
}

}
