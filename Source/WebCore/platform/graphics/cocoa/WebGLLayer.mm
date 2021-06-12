/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"

#if ENABLE(WEBGL)
#import "WebGLLayer.h"

#import "GraphicsLayer.h"
#import "GraphicsLayerCA.h"
#import "PlatformCALayer.h"
#import <pal/spi/cocoa/QuartzCoreSPI.h>

class WebGLLayerSwapChain final : public WebCore::GraphicsContextGLIOSurfaceSwapChain {
public:
    explicit WebGLLayerSwapChain(WebGLLayer* layer)  : m_layer(layer) { };
    ~WebGLLayerSwapChain() override = default;
    void present(Buffer&&) override;

    WebCore::IOSurface* displaySurface() { return m_displayBuffer.surface.get(); }
private:
    WebGLLayer* const m_layer;
};

@implementation WebGLLayer {
    BOOL _preparedForDisplay;
    std::optional<WebGLLayerSwapChain> _swapChain;
}

- (id)initWithDevicePixelRatio:(float)devicePixelRatio contentsOpaque:(bool)contentsOpaque
{
    self = [super init];
    self.transform = CATransform3DIdentity;
    self.contentsOpaque = contentsOpaque;
    self.contentsScale = devicePixelRatio;
    _swapChain.emplace(self);
    return self;
}

// When using an IOSurface as layer contents, we need to flip the
// layer to take into account that the IOSurface provides content
// in Y-up. This means that any incoming transform (unlikely, since
// this is a contents layer) and anchor point must add a Y scale of
// -1 and make sure the transform happens from the top.

- (void)setTransform:(CATransform3D)t
{
    [super setTransform:CATransform3DScale(t, 1, -1, 1)];
}

- (void)setAnchorPoint:(CGPoint)p
{
    [super setAnchorPoint:CGPointMake(p.x, 1.0 - p.y)];
}

- (CGImageRef)copyImageSnapshotWithColorSpace:(CGColorSpaceRef)colorSpace
{
    // FIXME: implement. https://bugs.webkit.org/show_bug.cgi?id=217377
    // When implementing, remember to use self.contentsScale.
    UNUSED_PARAM(colorSpace);
    return nullptr;
}
- (WebCore::GraphicsContextGLIOSurfaceSwapChain&) swapChain
{
    return _swapChain.value();
}

- (void)prepareForDisplay
{
    [self setNeedsDisplay];
    _preparedForDisplay = YES;
}

- (void)display
{
    if (_swapChain->displaySurface() && _preparedForDisplay) {
        self.contents = _swapChain->displaySurface()->asLayerContents();
        [self reloadValueForKeyPath:@"contents"];
    }
    auto layer = WebCore::PlatformCALayer::platformCALayerForLayer((__bridge void*)self);
    if (layer && layer->owner())
        layer->owner()->platformCALayerLayerDidDisplay(layer.get());

    _preparedForDisplay = NO;
}

@end

void WebGLLayerSwapChain::present(Buffer&& buffer)
{
    ASSERT(!m_spareBuffer.surface);
    GraphicsContextGLIOSurfaceSwapChain::present(WTFMove(buffer));
    [m_layer prepareForDisplay];
}

#endif // ENABLE(WEBGL)
