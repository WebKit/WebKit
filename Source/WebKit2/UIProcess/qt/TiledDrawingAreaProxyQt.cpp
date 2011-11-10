/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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
#include "TiledDrawingAreaProxy.h"

#include "QtSGUpdateQueue.h"
#include "QtViewInterface.h"
#include "ShareableBitmap.h"
#include "UpdateInfo.h"
#include "WKAPICast.h"
#include "WebPageProxy.h"

using namespace WebCore;

#define TILE_DEBUG_LOG

namespace WebKit {

void TiledDrawingAreaProxy::updateWebView(const Vector<IntRect>& paintedArea)
{
    // SG updates are triggered through QtSGUpdateQueue.
}

WebPageProxy* TiledDrawingAreaProxy::page()
{
    return m_webPageProxy;
}

void TiledDrawingAreaProxy::createTile(int tileID, const UpdateInfo& updateInfo)
{
    int nodeID = m_webView->sceneGraphUpdateQueue()->createTileNode(updateInfo.updateScaleFactor);
    m_tileNodeMap.set(tileID, nodeID);
    updateTile(tileID, updateInfo);
}

void TiledDrawingAreaProxy::updateTile(int tileID, const UpdateInfo& updateInfo)
{
    int nodeID = m_tileNodeMap.get(tileID);
    ASSERT(nodeID);

    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(updateInfo.bitmapHandle);
    // FIXME: We could avoid this copy by carying the ShareableBitmap all the way up to texture uploading.
    // Currently won't work since the SharedMemory handle is owned by updateInfo.
    QImage image(bitmap->createQImage().copy());
    QRect sourceRect(0, 0, updateInfo.updateRectBounds.width(), updateInfo.updateRectBounds.height());
    m_webView->sceneGraphUpdateQueue()->setNodeBackBuffer(nodeID, image, sourceRect, updateInfo.updateRectBounds);
}

void TiledDrawingAreaProxy::didRenderFrame()
{
    m_webView->sceneGraphUpdateQueue()->swapTileBuffers();
}

void TiledDrawingAreaProxy::removeTile(int tileID)
{
    int nodeID = m_tileNodeMap.take(tileID);
    m_webView->sceneGraphUpdateQueue()->removeTileNode(nodeID);
}

} // namespace WebKit
