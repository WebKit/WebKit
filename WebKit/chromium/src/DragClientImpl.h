/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef DragClientImpl_h
#define DragClientImpl_h

#include "DragActions.h"
#include "DragClient.h"

namespace WebCore {
class ClipBoard;
class DragData;
class IntPoint;
class KURL;
}

namespace WebKit {
class WebViewImpl;

class DragClientImpl : public WebCore::DragClient {
public:
    DragClientImpl(WebViewImpl* webView) : m_webView(webView) { }

    virtual void willPerformDragDestinationAction(
        WebCore::DragDestinationAction, WebCore::DragData*);
    virtual void willPerformDragSourceAction(
        WebCore::DragSourceAction, const WebCore::IntPoint&, WebCore::Clipboard*);
    virtual WebCore::DragDestinationAction actionMaskForDrag(WebCore::DragData*);
    virtual WebCore::DragSourceAction dragSourceActionMaskForPoint(
        const WebCore::IntPoint& windowPoint);
    virtual void startDrag(
        WebCore::DragImageRef dragImage,
        const WebCore::IntPoint& dragImageOrigin,
        const WebCore::IntPoint& eventPos,
        WebCore::Clipboard* clipboard,
        WebCore::Frame* frame,
        bool isLinkDrag = false);
    virtual WebCore::DragImageRef createDragImageForLink(
        WebCore::KURL&, const WebCore::String& label, WebCore::Frame*);
    virtual void dragControllerDestroyed();

private:
    WebViewImpl* m_webView;
};

} // namespace WebKit

#endif
