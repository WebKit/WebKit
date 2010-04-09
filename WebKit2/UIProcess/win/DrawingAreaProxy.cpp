/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "DrawingAreaProxy.h"

#include "Connection.h"
#include "DrawingAreaMessageKinds.h"
#include "DrawingAreaProxyMessageKinds.h"
#include "MessageID.h"
#include "UpdateChunk.h"
#include "WebProcessProxy.h"
#include "WebView.h"
#include <WebCore/BitmapInfo.h>
#include <WebCore/IntRect.h>

using namespace WebCore;

namespace WebKit {

DrawingAreaProxy::DrawingAreaProxy(WebView* webView)
    : m_webView(webView)
{
    m_backingStoreSize.cx = 0;
    m_backingStoreSize.cy = 0;
}

DrawingAreaProxy::~DrawingAreaProxy()
{
}

void DrawingAreaProxy::ensureBackingStore()
{
    if (m_backingStoreBitmap)
        return;

    BitmapInfo bitmapInfo = BitmapInfo::createBottomUp(IntSize(m_backingStoreSize));

    void* pixels = 0;
    m_backingStoreBitmap.set(::CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0));

    if (!m_backingStoreDC) {
        // Create a DC for the backing store.
        HDC screenDC = ::GetDC(0);
        m_backingStoreDC.set(::CreateCompatibleDC(screenDC));
        ::ReleaseDC(0, screenDC);
    }

    ::SelectObject(m_backingStoreDC.get(), m_backingStoreBitmap.get());
}

void DrawingAreaProxy::resize(SIZE size)
{
    m_backingStoreSize = size;
    m_backingStoreBitmap.clear();

    WebPageProxy* webPage = m_webView->page();
    webPage->process()->connection()->send(DrawingAreaMessage::SetFrame, webPage->pageID(), CoreIPC::In(IntSize(size.cx, size.cy)));
}

void DrawingAreaProxy::paint(HDC hdc, RECT dirtyRect)
{
    if (!m_backingStoreBitmap)
        return;

    // BitBlt from the backing-store to the passed in hdc.
    IntRect rect(dirtyRect);
    ::BitBlt(hdc, rect.x(), rect.y(), rect.width(), rect.height(), m_backingStoreDC.get(), rect.x(), rect.y(), SRCCOPY);
}

void DrawingAreaProxy::drawUpdateChunkIntoBackingStore(UpdateChunk& updateChunk)
{
    ensureBackingStore();

    OwnPtr<HDC> updateChunkBitmapDC(::CreateCompatibleDC(m_backingStoreDC.get()));

    // Create a bitmap.
    BitmapInfo bitmapInfo = BitmapInfo::createBottomUp(updateChunk.frame().size());

    // Duplicate the update chunk handle.
    HANDLE updateChunkHandle;
    BOOL result = ::DuplicateHandle(m_webView->page()->process()->processIdentifier(), updateChunk.memory(),
                                    ::GetCurrentProcess(), &updateChunkHandle, STANDARD_RIGHTS_REQUIRED | FILE_MAP_READ | FILE_MAP_WRITE, false, DUPLICATE_CLOSE_SOURCE);

    void* pixels = 0;
    OwnPtr<HBITMAP> hBitmap(::CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, &pixels, updateChunkHandle, 0));
    ::SelectObject(updateChunkBitmapDC.get(), hBitmap.get());

    // BitBlt from the UpdateChunk to the backing store.
    ::BitBlt(m_backingStoreDC.get(), updateChunk.frame().x(), updateChunk.frame().y(), updateChunk.frame().width(), updateChunk.frame().height(), updateChunkBitmapDC.get(), 0, 0, SRCCOPY);

    // FIXME: We should not do this here.
    ::CloseHandle(updateChunkHandle);

    // Invalidate the WebView's HWND.
    RECT rect = updateChunk.frame();
    ::InvalidateRect(m_webView->window(), &rect, false);
}

void DrawingAreaProxy::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder& arguments)
{
    switch (messageID.get<DrawingAreaProxyMessage::Kind>()) {
        case DrawingAreaProxyMessage::Update: {
            UpdateChunk updateChunk;
            if (!arguments.decode(updateChunk))
                return;
            
            drawUpdateChunkIntoBackingStore(updateChunk);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
    }
}

} // namespace WebKit
