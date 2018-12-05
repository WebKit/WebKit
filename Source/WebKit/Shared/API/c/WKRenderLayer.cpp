/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "WKRenderLayer.h"

#include "APIArray.h"
#include "WKAPICast.h"
#include "WebRenderLayer.h"

WKTypeID WKRenderLayerGetTypeID()
{
    return WebKit::toAPI(WebKit::WebRenderLayer::APIType);
}

WKRenderObjectRef WKRenderLayerGetRenderer(WKRenderLayerRef renderLayerRef)
{
    return toAPI(WebKit::toImpl(renderLayerRef)->renderer());
}

WKStringRef WKRenderLayerCopyRendererName(WKRenderLayerRef renderLayerRef)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(renderLayerRef)->renderer()->name());
}

WKStringRef WKRenderLayerCopyElementTagName(WKRenderLayerRef renderLayerRef)
{
    WebKit::WebRenderLayer* renderLayer = WebKit::toImpl(renderLayerRef);
    if (!renderLayer->renderer()->elementTagName().isNull())
        return WebKit::toCopiedAPI(renderLayer->renderer()->elementTagName());

    return nullptr;
}

WKStringRef WKRenderLayerCopyElementID(WKRenderLayerRef renderLayerRef)
{
    WebKit::WebRenderLayer* renderLayer = WebKit::toImpl(renderLayerRef);
    if (!renderLayer->renderer()->elementID().isNull())
        return WebKit::toCopiedAPI(renderLayer->renderer()->elementID());

    return nullptr;
}

WKArrayRef WKRenderLayerGetElementClassNames(WKRenderLayerRef renderLayerRef)
{
    return WebKit::toAPI(WebKit::toImpl(renderLayerRef)->renderer()->elementClassNames());
}

WKRect WKRenderLayerGetAbsoluteBounds(WKRenderLayerRef renderLayerRef)
{
    WebCore::IntRect bounds = WebKit::toImpl(renderLayerRef)->absoluteBoundingBox();
    return WKRectMake(bounds.x(), bounds.y(), bounds.width(), bounds.height());
}

bool WKRenderLayerIsClipping(WKRenderLayerRef renderLayerRef)
{
    return WebKit::toImpl(renderLayerRef)->isClipping();
}

bool WKRenderLayerIsClipped(WKRenderLayerRef renderLayerRef)
{
    return WebKit::toImpl(renderLayerRef)->isClipped();
}

bool WKRenderLayerIsReflection(WKRenderLayerRef renderLayerRef)
{
    return WebKit::toImpl(renderLayerRef)->isReflection();
}

WKCompositingLayerType WKRenderLayerGetCompositingLayerType(WKRenderLayerRef renderLayerRef)
{
    switch (WebKit::toImpl(renderLayerRef)->compositingLayerType()) {
    case WebKit::WebRenderLayer::None:
        return kWKCompositingLayerTypeNone;
    case WebKit::WebRenderLayer::Normal:
        return kWKCompositingLayerTypeNormal;
    case WebKit::WebRenderLayer::Tiled:
        return kWKCompositingLayerTypeTiled;
    case WebKit::WebRenderLayer::Media:
        return kWKCompositingLayerTypeMedia;
    case WebKit::WebRenderLayer::Container:
        return kWKCompositingLayerTypeContainer;
    }

    ASSERT_NOT_REACHED();
    return kWKCompositingLayerTypeNone;
}

WK_EXPORT double WKRenderLayerGetBackingStoreMemoryEstimate(WKRenderLayerRef renderLayerRef)
{
    return WebKit::toImpl(renderLayerRef)->backingStoreMemoryEstimate();
}

WKArrayRef WKRenderLayerGetNegativeZOrderList(WKRenderLayerRef renderLayerRef)
{
    return WebKit::toAPI(WebKit::toImpl(renderLayerRef)->negativeZOrderList());
}

WKArrayRef WKRenderLayerGetNormalFlowList(WKRenderLayerRef renderLayerRef)
{
    return WebKit::toAPI(WebKit::toImpl(renderLayerRef)->normalFlowList());
}

WKArrayRef WKRenderLayerGetPositiveZOrderList(WKRenderLayerRef renderLayerRef)
{
    return WebKit::toAPI(WebKit::toImpl(renderLayerRef)->positiveZOrderList());
}

WKRenderLayerRef WKRenderLayerGetFrameContentsLayer(WKRenderLayerRef renderLayerRef)
{
    return toAPI(WebKit::toImpl(renderLayerRef)->frameContentsLayer());
}
