/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WebDragClient.h"

#include "ClipboardQt.h"
#include "DragData.h"
#include "GraphicsContext.h"
#include "ShareableBitmap.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"

using namespace WebCore;

namespace WebKit {

static PassRefPtr<ShareableBitmap> convertQPixmapToShareableBitmap(QPixmap* pixmap)
{
    // FIXME: We need this hack until https://bugs.webkit.org/show_bug.cgi?id=60621 is fixed.
    // We cannot pass a null handle so create a pixmap size 1x1 and send that instead.
    QPixmap p(QSize(1, 1));
    if (pixmap)
        p = *pixmap;
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(IntSize(p.size()), ShareableBitmap::SupportsAlpha);
    OwnPtr<GraphicsContext> graphicsContext = bitmap->createGraphicsContext();

    graphicsContext->platformContext()->drawPixmap(0, 0, p);
    return bitmap.release();
}

void WebDragClient::startDrag(DragImageRef dragImage, const IntPoint& clientPosition, const IntPoint& globalPosition, Clipboard* clipboard, Frame*, bool)
{
#ifndef QT_NO_DRAGANDDROP
    QMimeData* clipboardData = static_cast<ClipboardQt*>(clipboard)->clipboardData();
    DragOperation dragOperationMask = clipboard->sourceOperation();
    static_cast<ClipboardQt*>(clipboard)->invalidateWritableData();
    DragData dragData(clipboardData, clientPosition, globalPosition, dragOperationMask);

    RefPtr<ShareableBitmap> bitmap = convertQPixmapToShareableBitmap(dragImage);
    ShareableBitmap::Handle handle;
    if (!bitmap->createHandle(handle))
        return;

    m_page->send(Messages::WebPageProxy::StartDrag(dragData, handle));
#else
    Q_UNUSED(dragImage)
    Q_UNUSED(clientPosition)
    Q_UNUSED(globalPosition)
    Q_UNUSED(clipboard)
#endif
}

}
