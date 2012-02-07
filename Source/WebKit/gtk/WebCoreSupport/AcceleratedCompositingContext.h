/*
 * Copyright (C) 2011 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef AcceleratedCompositingContext_h
#define AcceleratedCompositingContext_h

#include "GraphicsLayer.h"
#include "GraphicsLayerClient.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Timer.h"
#include "webkitwebview.h"
#include <wtf/PassOwnPtr.h>

#if USE(TEXTURE_MAPPER_GL)
#include "TextureMapperNode.h"
#include "WindowGLContext.h"
#endif

#if USE(ACCELERATED_COMPOSITING)

namespace WebKit {

class AcceleratedCompositingContext : public WebCore::GraphicsLayerClient {
    WTF_MAKE_NONCOPYABLE(AcceleratedCompositingContext);
public:
    static PassOwnPtr<AcceleratedCompositingContext> create(WebKitWebView* webView)
    {
        return adoptPtr(new AcceleratedCompositingContext(webView));
    }

    virtual ~AcceleratedCompositingContext();
    void attachRootGraphicsLayer(WebCore::GraphicsLayer*);
    void scheduleRootLayerRepaint(const WebCore::IntRect&);
    void markForSync();
    void syncLayersTimeout(WebCore::Timer<AcceleratedCompositingContext>*);
    void syncLayersNow();
    void resizeRootLayer(const WebCore::IntSize&);
    bool renderLayersToWindow(const WebCore::IntRect& clipRect);
    bool enabled();

    // GraphicsLayerClient
    virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double time);
    virtual void notifySyncRequired(const WebCore::GraphicsLayer*);
    virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::IntRect& rectToPaint);
    virtual bool showDebugBorders(const WebCore::GraphicsLayer*) const;
    virtual bool showRepaintCounter(const WebCore::GraphicsLayer*) const;

private:
    WebKitWebView* m_webView;
    OwnPtr<WebCore::GraphicsLayer> m_rootGraphicsLayer;
    WebCore::Timer<AcceleratedCompositingContext> m_syncTimer;

#if USE(CLUTTER)
    GtkWidget* m_rootLayerEmbedder;
#elif USE(TEXTURE_MAPPER_GL)
    void initializeIfNecessary();

    bool m_initialized;
    WebCore::TextureMapperNode* m_rootTextureMapperNode;
    OwnPtr<WebCore::WindowGLContext> m_context;
    OwnPtr<WebCore::TextureMapper> m_textureMapper;
#endif

    AcceleratedCompositingContext(WebKitWebView*);
};

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING)
#endif // AcceleratedCompositingContext_h
