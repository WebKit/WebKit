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
#include "platform/WebLayerTreeView.h"

#include "WebLayerTreeViewImpl.h"
#include "WebRect.h"
#include "WebSize.h"
#include "platform/WebLayer.h"
#include "cc/CCLayerTreeHost.h"

using namespace WebCore;

namespace WebKit {
WebLayerTreeView::Settings::operator CCSettings() const
{
    CCSettings settings;
    settings.acceleratePainting = acceleratePainting;
    settings.compositeOffscreen = compositeOffscreen;
    settings.showFPSCounter = showFPSCounter;
    settings.showPlatformLayerTree = showPlatformLayerTree;

    // FIXME: showFPSCounter / showPlatformLayerTree aren't supported currently.
    settings.showFPSCounter = false;
    settings.showPlatformLayerTree = false;
    return settings;
}

WebLayerTreeView WebLayerTreeView::create(WebLayerTreeViewClient* client, const WebLayer& root, const WebLayerTreeView::Settings& settings)
{
    return WebLayerTreeView(WebLayerTreeViewImpl::create(client, root, settings));
}

void WebLayerTreeView::reset()
{
    m_private.reset();
}

void WebLayerTreeView::assign(const WebLayerTreeView& other)
{
    m_private = other.m_private;
}

bool WebLayerTreeView::equals(const WebLayerTreeView& n) const
{
    return (m_private.get() == n.m_private.get());
}

void WebLayerTreeView::composite()
{
    if (CCProxy::hasImplThread())
        m_private->setNeedsCommit();
    else
        m_private->composite();
}

void WebLayerTreeView::setViewportSize(const WebSize& viewportSize)
{
    m_private->setViewport(viewportSize);
}

WebSize WebLayerTreeView::viewportSize() const
{
    return WebSize(m_private->viewportSize());
}

bool WebLayerTreeView::compositeAndReadback(void *pixels, const WebRect& rect)
{
    return m_private->compositeAndReadback(pixels, rect);
}

void WebLayerTreeView::setRootLayer(WebLayer *root)
{
    if (root)
        m_private->setRootLayer(*root);
    else
        m_private->setRootLayer(PassRefPtr<LayerChromium>());
}

WebLayerTreeView::WebLayerTreeView(const PassRefPtr<CCLayerTreeHost>& node)
    : m_private(node)
{
}

WebLayerTreeView& WebLayerTreeView::operator=(const PassRefPtr<CCLayerTreeHost>& node)
{
    m_private = node;
    return *this;
}

WebLayerTreeView::operator PassRefPtr<CCLayerTreeHost>() const
{
    return m_private.get();
}

} // namespace WebKit
