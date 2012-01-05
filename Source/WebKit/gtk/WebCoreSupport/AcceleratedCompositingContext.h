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

#include "GraphicsLayerClient.h"
#include "IntRect.h"
#include "IntSize.h"
#include "Timer.h"
#include "webkitwebview.h"
#include <wtf/PassOwnPtr.h>

#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {
class GraphicsLayer;
}

namespace WebKit {

class AcceleratedCompositingContext {
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
    void resizeRootLayer(const WebCore::IntSize&);

private:
    WebKitWebView* m_webView;
    OwnPtr<WebCore::GraphicsLayer> m_rootGraphicsLayer;
    WebCore::Timer<AcceleratedCompositingContext> m_syncTimer;

#if USE(CLUTTER)
    GtkWidget* m_rootLayerEmbedder;
#endif

    AcceleratedCompositingContext(WebKitWebView*);
};

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING)
#endif // AcceleratedCompositingContext_h
