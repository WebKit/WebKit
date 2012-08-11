/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebLayerTreeViewImpl.h"

#include "LayerChromium.h"
#include "cc/CCLayerTreeHost.h"
#include "cc/CCThreadProxy.h"
#include <public/WebGraphicsContext3D.h>
#include <public/WebLayer.h>
#include <public/WebLayerTreeView.h>
#include <public/WebLayerTreeViewClient.h>
#include <public/WebSize.h>
#include <public/WebThread.h>

using namespace WebCore;

namespace {

// Adapts a pure WebGraphicsContext3D into a WebCompositorOutputSurface until
// downstream code can be updated to produce output surfaces directly.
class WebGraphicsContextToOutputSurfaceAdapter : public WebKit::WebCompositorOutputSurface {
public:
    explicit WebGraphicsContextToOutputSurfaceAdapter(PassOwnPtr<WebKit::WebGraphicsContext3D> context)
        : m_context3D(context)
        , m_client(0)
    {
    }

    virtual bool bindToClient(WebKit::WebCompositorOutputSurfaceClient* client) OVERRIDE
    {
        ASSERT(client);
        if (!m_context3D->makeContextCurrent())
            return false;
        m_client = client;
        return true;
    }

    virtual const Capabilities& capabilities() const OVERRIDE
    {
        return m_capabilities;
    }

    virtual WebKit::WebGraphicsContext3D* context3D() const OVERRIDE
    {
        return m_context3D.get();
    }

    virtual void sendFrameToParentCompositor(const WebKit::WebCompositorFrame&) OVERRIDE
    {
    }

private:
    OwnPtr<WebKit::WebGraphicsContext3D> m_context3D;
    Capabilities m_capabilities;
    WebKit::WebCompositorOutputSurfaceClient* m_client;
};

}

namespace WebKit {

// Converts messages from CCLayerTreeHostClient to WebLayerTreeViewClient.
class WebLayerTreeViewClientAdapter : public WebCore::CCLayerTreeHostClient {
public:
    WebLayerTreeViewClientAdapter(WebLayerTreeViewClient* client)
        : m_client(client)
        , m_usingRealOutputSurface(false)
    {
    }
    virtual ~WebLayerTreeViewClientAdapter() { }

    // CCLayerTreeHostClient implementation
    virtual void willBeginFrame() OVERRIDE { m_client->willBeginFrame(); }
    virtual void didBeginFrame() OVERRIDE { m_client->didBeginFrame(); }
    virtual void updateAnimations(double monotonicFrameBeginTime) OVERRIDE { m_client->updateAnimations(monotonicFrameBeginTime); }
    virtual void layout() OVERRIDE { m_client->layout(); }
    virtual void applyScrollAndScale(const WebCore::IntSize& scrollDelta, float pageScale) OVERRIDE { m_client->applyScrollAndScale(scrollDelta, pageScale); }
    virtual PassOwnPtr<WebCompositorOutputSurface> createOutputSurface() OVERRIDE
    {
        WebCompositorOutputSurface* outputSurface = m_client->createOutputSurface();
        if (outputSurface) {
            m_usingRealOutputSurface = true;
            return adoptPtr(outputSurface);
        }

        // Temporarily, if the output surface can't be created, create a WebGraphicsContext3D
        // directly. This allows bootstrapping the output surface system while downstream
        // users of the API still use the old approach.
        WebGraphicsContext3D* context = m_client->createContext3D();
        if (!context)
            return nullptr;

        m_usingRealOutputSurface = false;
        return adoptPtr(new WebGraphicsContextToOutputSurfaceAdapter(adoptPtr(context)));
    }

    virtual void didRecreateOutputSurface(bool success) OVERRIDE
    {
        if (m_usingRealOutputSurface) {
            m_client->didRecreateOutputSurface(success);
            return;
        }
        m_client->didRebindGraphicsContext(success);
    }
    virtual void willCommit() OVERRIDE { m_client->willCommit(); }
    virtual void didCommit() OVERRIDE { m_client->didCommit(); }
    virtual void didCommitAndDrawFrame() OVERRIDE { m_client->didCommitAndDrawFrame(); }
    virtual void didCompleteSwapBuffers() OVERRIDE { m_client->didCompleteSwapBuffers(); }
    virtual void scheduleComposite() OVERRIDE { m_client->scheduleComposite(); }

private:
    WebLayerTreeViewClient* m_client;
    bool m_usingRealOutputSurface;
};

PassOwnPtr<WebLayerTreeViewImpl> WebLayerTreeViewImpl::create(WebLayerTreeViewClient* client, const WebLayer& root, const WebLayerTreeView::Settings& settings)
{
    OwnPtr<WebLayerTreeViewImpl> impl = adoptPtr(new WebLayerTreeViewImpl(client, settings));
    if (!impl->layerTreeHost())
        return nullptr;
    impl->layerTreeHost()->setRootLayer(root);
    return impl.release();
}

WebLayerTreeViewImpl::WebLayerTreeViewImpl(WebLayerTreeViewClient* client, const WebLayerTreeView::Settings& settings) 
    : m_clientAdapter(adoptPtr(new WebLayerTreeViewClientAdapter(client)))
    , m_layerTreeHost(CCLayerTreeHost::create(m_clientAdapter.get(), settings))
{
}

WebLayerTreeViewImpl::~WebLayerTreeViewImpl()
{
}

} // namespace WebKit
