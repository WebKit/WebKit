/*
 * Copyright (C) 2006 Dirk Mueller <mueller@kde.org>
 * Copyright (C) 2006 Zack Rusin <zack@kde.org>
 * Copyright (C) 2006 George Staikos <staikos@kde.org>
 * Copyright (C) 2006 Simon Hausmann <hausmann@kde.org>
 * Copyright (C) 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * All rights reserved.
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
#include "FrameQt.h"

#include "Element.h"
#include "RenderObject.h"
#include "RenderWidget.h"
#include "RenderLayer.h"
#include "Page.h"
#include "Document.h"
#include "HTMLElement.h"
#include "DOMWindow.h"
#include "EditorClientQt.h"
#include "FrameLoadRequest.h"
#include "DOMImplementation.h"
#include "ResourceHandleInternal.h"
#include "Document.h"
#include "Settings.h"
#include "Plugin.h"
#include "FramePrivate.h"
#include "GraphicsContext.h"
#include "HTMLDocument.h"
#include "ResourceHandle.h"
#include "PlatformMouseEvent.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformWheelEvent.h"
#include "MouseEventWithHitTestResults.h"
#include "SelectionController.h"
#include "TypingCommand.h"
#include "JSLock.h"
#include "kjs_window.h"
#include "runtime_root.h"

#include <QScrollArea>

#define notImplemented() do { fprintf(stderr, "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); } while(0)

namespace WebCore {

void FrameLoader::submitForm(const FrameLoadRequest& frameLoadRequest, Event*)
{
    // FIXME: We'd like to remove this altogether and fix the multiple form submission issue another way.
    // We do not want to submit more than one form from the same page,
    // nor do we want to submit a single form more than once.
    // This flag prevents these from happening; not sure how other browsers prevent this.
    // The flag is reset in each time we start handle a new mouse or key down event, and
    // also in setView since this part may get reused for a page from the back/forward cache.
    // The form multi-submit logic here is only needed when we are submitting a form that affects this frame.
    // FIXME: Frame targeting is only one of the ways the submission could end up doing something other
    // than replacing this frame's content, so this check is flawed. On the other hand, the check is hardly
    // needed any more now that we reset m_submittedFormURL on each mouse or key down event.
    Frame* target = m_frame->tree()->find(request.frameName());
    if (m_frame->tree()->isDescendantOf(target)) {
        if (m_submittedFormURL == request.resourceRequest().url())
            return;
        m_submittedFormURL = request.resourceRequest().url();
    }

    if (QtFrame(m_frame)->client())
        QtFrame(m_frame)->client()->submitForm(request.resourceRequest().httpMethod(),
            request.resourceRequest().url(), &request.resourceRequest().httpBody());

    clearRecordedFormValues();
}

void FrameLoader::urlSelected(const FrameLoadRequest& frameLoadRequest, Event*)
{
    const ResourceRequest& request = frameLoadRequest.resourceRequest();

    if (!QtFrame(m_frame)->client())
        return;

    QtFrame(m_frame)->client()->openURL(request.url());
}

void FrameLoader::setTitle(const String& title)
{
    if (m_frame->view() && m_frame->view()->parentWidget())
        m_frame->view()->parentWidget()->setWindowTitle(title);
}

Frame* FrameLoader::createFrame(const KURL& url, const String& name, Element* ownerElement, const String& referrer)
{
    notImplemented();
    return 0;
}

ObjectContentType FrameLoader::objectContentType(const KURL&, const String& mimeType)
{
    notImplemented();
    return ObjectContentType();
}

Widget* FrameLoader::createPlugin(Element*, const KURL&, const Vector<String>&, const Vector<String>&, const String&)
{
    notImplemented();
    return 0;
}

Widget* FrameLoader::createJavaAppletWidget(const IntSize&, Element*, const HashMap<String, String>&)
{
    notImplemented();
    return 0;
}

KURL FrameLoader::originalRequestURL() const
{
    notImplemented();
    return KURL();
}

}

// vim: ts=4 sw=4 et
