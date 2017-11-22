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

static FrameView* getFrameViewAndLayoutIfNonNull(Frame* frame)
{
    auto* view = frame ? frame->view() : nullptr;
    if (view) {
        ASSERT(frame->pageZoomFactor());
        frame->document()->updateLayoutIgnorePendingStylesheets(Document::RunPostLayoutTasks::Synchronously);
    }
    return view;
}

double VisualViewport::offsetLeft() const
{
    if (auto* view = getFrameViewAndLayoutIfNonNull(m_frame))
        return (view->visualViewportRect().x() - view->layoutViewportRect().x()) / m_frame->pageZoomFactor();
    return 0;
}

double VisualViewport::offsetTop() const
{
    if (auto* view = getFrameViewAndLayoutIfNonNull(m_frame))
        return (view->visualViewportRect().y() - view->layoutViewportRect().y()) / m_frame->pageZoomFactor();
    return 0;
}

double VisualViewport::pageLeft() const
{
    if (auto* view = getFrameViewAndLayoutIfNonNull(m_frame))
        return view->visualViewportRect().x() / m_frame->pageZoomFactor();
    return 0;
}

double VisualViewport::pageTop() const
{
    if (auto* view = getFrameViewAndLayoutIfNonNull(m_frame))
        return view->visualViewportRect().y() / m_frame->pageZoomFactor();
    return 0;
}

double VisualViewport::width() const
{
    if (auto* view = getFrameViewAndLayoutIfNonNull(m_frame))
        return view->visualViewportRect().width() / m_frame->pageZoomFactor();
    return 0;
}

double VisualViewport::height() const
{
    if (auto* view = getFrameViewAndLayoutIfNonNull(m_frame))
        return view->visualViewportRect().height() / m_frame->pageZoomFactor();
    return 0;
}

double VisualViewport::scale() const
{
    // Subframes always have scale 1 since they aren't scaled relative to their parent frame.
    if (!m_frame || !m_frame->isMainFrame())
        return 1;

    if (auto* page = m_frame->page()) {
        m_frame->document()->updateLayoutIgnorePendingStylesheets(Document::RunPostLayoutTasks::Synchronously);
        return page->pageScaleFactor();
    }
    return 1;
}

} // namespace WebCore
