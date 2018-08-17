/*
 * Copyright (C) 2017 Google Inc. All rights reserved.
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
#include "VisualViewport.h"

#include "ContextDestructionObserver.h"
#include "DOMWindow.h"
#include "Document.h"
#include "DocumentEventQueue.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "Page.h"

namespace WebCore {

VisualViewport::VisualViewport(Frame* frame)
    : DOMWindowProperty(frame)
{
}

EventTargetInterface VisualViewport::eventTargetInterface() const
{
    return VisualViewportEventTargetInterfaceType;
}

ScriptExecutionContext* VisualViewport::scriptExecutionContext() const
{
    if (!m_associatedDOMWindow)
        return nullptr;
    return static_cast<ContextDestructionObserver*>(m_associatedDOMWindow)->scriptExecutionContext();
}

bool VisualViewport::addEventListener(const AtomicString& eventType, Ref<EventListener>&& listener, const AddEventListenerOptions& options)
{
    if (!EventTarget::addEventListener(eventType, WTFMove(listener), options))
        return false;

    if (m_frame)
        m_frame->document()->addListenerTypeIfNeeded(eventType);
    return true;
}

void VisualViewport::updateFrameLayout() const
{
    ASSERT(m_frame);
    m_frame->document()->updateLayoutIgnorePendingStylesheets(Document::RunPostLayoutTasks::Synchronously);
}

double VisualViewport::offsetLeft() const
{
    if (!m_frame)
        return 0;

    updateFrameLayout();
    return m_offsetLeft;
}

double VisualViewport::offsetTop() const
{
    if (!m_frame)
        return 0;

    updateFrameLayout();
    return m_offsetTop;
}

double VisualViewport::pageLeft() const
{
    if (!m_frame)
        return 0;

    updateFrameLayout();
    return m_pageLeft;
}

double VisualViewport::pageTop() const
{
    if (!m_frame)
        return 0;

    updateFrameLayout();
    return m_pageTop;
}

double VisualViewport::width() const
{
    if (!m_frame)
        return 0;

    updateFrameLayout();
    return m_width;
}

double VisualViewport::height() const
{
    if (!m_frame)
        return 0;

    updateFrameLayout();
    return m_height;
}

double VisualViewport::scale() const
{
    // Subframes always have scale 1 since they aren't scaled relative to their parent frame.
    if (!m_frame || !m_frame->isMainFrame())
        return 1;

    updateFrameLayout();
    return m_scale;
}

void VisualViewport::update()
{
    double offsetLeft = 0;
    double offsetTop = 0;
    m_pageLeft = 0;
    m_pageTop = 0;
    double width = 0;
    double height = 0;
    double scale = 1;

    if (m_frame) {
        if (auto* view = m_frame->view()) {
            auto visualViewportRect = view->visualViewportRect();
            auto layoutViewportRect = view->layoutViewportRect();
            auto pageZoomFactor = m_frame->pageZoomFactor();
            ASSERT(pageZoomFactor);
            offsetLeft = (visualViewportRect.x() - layoutViewportRect.x()) / pageZoomFactor;
            offsetTop = (visualViewportRect.y() - layoutViewportRect.y()) / pageZoomFactor;
            m_pageLeft = visualViewportRect.x() / pageZoomFactor;
            m_pageTop = visualViewportRect.y() / pageZoomFactor;
            width = visualViewportRect.width() / pageZoomFactor;
            height = visualViewportRect.height() / pageZoomFactor;
        }
        if (auto* page = m_frame->page())
            scale = page->pageScaleFactor();
    }

    if (m_offsetLeft != offsetLeft || m_offsetTop != offsetTop) {
        enqueueScrollEvent();
        m_offsetLeft = offsetLeft;
        m_offsetTop = offsetTop;
    }
    if (m_width != width || m_height != height || m_scale != scale) {
        enqueueResizeEvent();
        m_width = width;
        m_height = height;
        m_scale = scale;
    }
}

void VisualViewport::enqueueResizeEvent()
{
    if (!m_frame)
        return;

    m_frame->document()->eventQueue().enqueueResizeEvent(*this, Event::CanBubble::No, Event::IsCancelable::No);
}

void VisualViewport::enqueueScrollEvent()
{
    if (!m_frame)
        return;

    m_frame->document()->eventQueue().enqueueScrollEvent(*this, Event::CanBubble::No, Event::IsCancelable::No);
}

} // namespace WebCore
