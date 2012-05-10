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

#include "config.h"
#include "DragClientImpl.h"
#include "DragImageRef.h"
#include "ChromiumDataObject.h"
#include "ClipboardChromium.h"
#include "Frame.h"
#include "NativeImageSkia.h"
#include "platform/WebCommon.h"
#include "platform/WebImage.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"

#include <public/WebDragData.h>

using namespace WebCore;

namespace WebKit {

void DragClientImpl::willPerformDragDestinationAction(DragDestinationAction, DragData*)
{
    // FIXME
}

void DragClientImpl::willPerformDragSourceAction(DragSourceAction, const IntPoint&, Clipboard*)
{
    // FIXME
}

DragDestinationAction DragClientImpl::actionMaskForDrag(DragData*)
{
    if (m_webView->client() && m_webView->client()->acceptsLoadDrops())
        return DragDestinationActionAny;

    return static_cast<DragDestinationAction>(
        DragDestinationActionDHTML | DragDestinationActionEdit);
}

DragSourceAction DragClientImpl::dragSourceActionMaskForPoint(const IntPoint& windowPoint)
{
    // We want to handle drag operations for all source types.
    return DragSourceActionAny;
}

void DragClientImpl::startDrag(DragImageRef dragImage,
                               const IntPoint& dragImageOrigin,
                               const IntPoint& eventPos,
                               Clipboard* clipboard,
                               Frame* frame,
                               bool isLinkDrag)
{
    // Add a ref to the frame just in case a load occurs mid-drag.
    RefPtr<Frame> frameProtector = frame;

    WebDragData dragData = static_cast<ClipboardChromium*>(clipboard)->dataObject();

    DragOperation dragOperationMask = clipboard->sourceOperation();

    IntSize offsetSize(eventPos - dragImageOrigin);
    WebPoint offsetPoint(offsetSize.width(), offsetSize.height());
    m_webView->startDragging(
        dragData, static_cast<WebDragOperationsMask>(dragOperationMask),
        dragImage ? WebImage(*dragImage) : WebImage(),
        offsetPoint);
}

void DragClientImpl::dragControllerDestroyed()
{
    // Our lifetime is bound to the WebViewImpl.
}

} // namespace WebKit
