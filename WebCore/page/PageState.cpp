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
#include "Frame.h"
#include "FrameView.h"
#include "Page.h"
#include "kjs_window.h"
#include <kjs/JSLock.h>
#include <kjs/property_map.h>
#include <kjs/SavedBuiltins.h>

using KJS::Collector;
using KJS::JSLock;
using KJS::SavedBuiltins;
using KJS::SavedProperties;

namespace WebCore {

PassRefPtr<PageState> PageState::create(Page* page)
{
    return new PageState(page);
}

PageState::PageState(Page* page)
    : m_document(page->mainFrame()->document())
    , m_view(page->mainFrame()->view())
    , m_mousePressNode(page->mainFrame()->mousePressNode())
    , m_URL(page->mainFrame()->url())
    , m_windowProperties(new SavedProperties)
    , m_locationProperties(new SavedProperties)
    , m_interpreterBuiltins(new SavedBuiltins)
{
    Frame* mainFrame = page->mainFrame();

    mainFrame->clearTimers();

    JSLock lock;

    mainFrame->saveWindowProperties(m_windowProperties.get());
    mainFrame->saveLocationProperties(m_locationProperties.get());
    mainFrame->saveInterpreterBuiltins(*m_interpreterBuiltins.get());
    m_pausedTimeouts.set(mainFrame->pauseTimeouts());

    m_document->setInPageCache(true);
}

PageState::~PageState()
{
    clear();
}

void PageState::restoreJavaScriptState(Page* page)
{
    Frame* mainFrame = page->mainFrame();

    JSLock lock;
    mainFrame->restoreWindowProperties(m_windowProperties.get());
    mainFrame->restoreLocationProperties(m_locationProperties.get());
    mainFrame->restoreInterpreterBuiltins(*m_interpreterBuiltins.get());
    mainFrame->resumeTimeouts(m_pausedTimeouts.get());
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
