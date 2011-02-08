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

#include "ShareableBitmap.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/BitmapInfo.h>
#include <WebCore/COMPtr.h>
#include <WebCore/ClipboardWin.h>
#include <WebCore/DragController.h>
#include <WebCore/Frame.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/Page.h>
#include <shlobj.h>

using namespace WebCore;

namespace WebKit {

static DWORD draggingSourceOperationMaskToDragCursors(DragOperation op)
{
    DWORD result = DROPEFFECT_NONE;
    if (op == DragOperationEvery)
        return DROPEFFECT_COPY | DROPEFFECT_LINK | DROPEFFECT_MOVE; 
    if (op & DragOperationCopy)
        result |= DROPEFFECT_COPY; 
    if (op & DragOperationLink)
        result |= DROPEFFECT_LINK; 
    if (op & DragOperationMove)
        result |= DROPEFFECT_MOVE;
    if (op & DragOperationGeneric)
        result |= DROPEFFECT_MOVE;
    return result;
}

void WebDragClient::startDrag(DragImageRef image, const IntPoint& imageOrigin, const IntPoint& dragPoint, Clipboard* clipboard, Frame* frame, bool isLink)
{
    COMPtr<IDataObject> dataObject = static_cast<ClipboardWin*>(clipboard)->dataObject();

    if (!dataObject)
        return;

    OwnPtr<HDC> bitmapDC(CreateCompatibleDC(0));
    BITMAPINFO bitmapInfo = {0};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    GetDIBits(bitmapDC.get(), image, 0, 0, 0, &bitmapInfo, DIB_RGB_COLORS);
    if (bitmapInfo.bmiHeader.biSizeImage <= 0)
        bitmapInfo.bmiHeader.biSizeImage = bitmapInfo.bmiHeader.biWidth * abs(bitmapInfo.bmiHeader.biHeight) * (bitmapInfo.bmiHeader.biBitCount + 7) / 8;
    
    RefPtr<SharedMemory> memoryBuffer = SharedMemory::create(bitmapInfo.bmiHeader.biSizeImage);

    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    GetDIBits(bitmapDC.get(), image, 0, bitmapInfo.bmiHeader.biHeight, memoryBuffer->data(), &bitmapInfo, DIB_RGB_COLORS);       

    SharedMemory::Handle handle;
    if (!memoryBuffer->createHandle(handle, SharedMemory::ReadOnly))
        return;
    DWORD okEffect = draggingSourceOperationMaskToDragCursors(m_page->corePage()->dragController()->sourceDragOperation());
    DragData dragData(dataObject.get(), IntPoint(), IntPoint(), DragOperationNone);
    m_page->send(Messages::WebPageProxy::StartDragDrop(imageOrigin, dragPoint, okEffect, dragData.dragDataMap(), IntSize(bitmapInfo.bmiHeader.biWidth, bitmapInfo.bmiHeader.biHeight), handle, isLink), m_page->pageID());
}

} // namespace WebKit
