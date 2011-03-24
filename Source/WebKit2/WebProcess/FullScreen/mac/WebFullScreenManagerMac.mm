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
#import "config.h"
#import "WebFullScreenManagerMac.h"

#if ENABLE(FULLSCREEN_API)

#import "Connection.h"
#import "LayerTreeContext.h"
#import "MessageID.h"
#import "WebFullScreenManagerProxyMessages.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <QuartzCore/QuartzCore.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/Page.h>
#import <WebCore/Settings.h>
#import <WebKitSystemInterface.h>

using namespace WebCore;

typedef void (WebKit::WebFullScreenManager::*AnimationBeganFunction)();
typedef void (WebKit::WebFullScreenManager::*AnimationFinishedFunction)(bool);

#if defined(BUILDING_ON_LEOPARD)
@interface CATransaction(SnowLeopardConvenienceFunctions)
+ (void)setDisableActions:(BOOL)flag;
@end

@implementation CATransaction(SnowLeopardConvenienceFunctions)
+ (void)setDisableActions:(BOOL)flag
{
    [self setValue:[NSNumber numberWithBool:flag] forKey:kCATransactionDisableActions];
}
@end
#endif

@interface WebFullScreenManagerAnimationListener : NSObject {
    WebKit::WebFullScreenManager* _manager;
    AnimationBeganFunction _began;
    AnimationFinishedFunction _finished;
}
- (id)initWithManager:(WebKit::WebFullScreenManager*)manager began:(AnimationBeganFunction)began finished:(AnimationFinishedFunction)finished;
- (void)animationDidStart:(CAAnimation *)anim;
- (void)animationDidStop:(CAAnimation *)anim finished:(BOOL)flag;
- (void)invalidate;
@end

@implementation WebFullScreenManagerAnimationListener
- (id)initWithManager:(WebKit::WebFullScreenManager*)manager began:(AnimationBeganFunction)began finished:(AnimationFinishedFunction)finished
{
    self = [super init];
    if (!self)
        return nil;

    _manager = manager;
    _began = began;
    _finished = finished;
    return self;
}

- (void)animationDidStart:(CAAnimation *)anim
{
    if (_manager && _began)
        (_manager->*_began)();
}

- (void)animationDidStop:(CAAnimation *)anim finished:(BOOL)flag
{
    if (_manager && _finished)
        (_manager->*_finished)(flag);
}

- (void)invalidate
{
    _manager = 0;
    _began = 0;
    _finished = 0;
}
@end

namespace WebKit {

PassRefPtr<WebFullScreenManager> WebFullScreenManager::create(WebPage* page)
{
    return WebFullScreenManagerMac::create(page);
}

PassRefPtr<WebFullScreenManagerMac> WebFullScreenManagerMac::create(WebPage* page)
{
    return adoptRef(new WebFullScreenManagerMac(page));
}

WebFullScreenManagerMac::WebFullScreenManagerMac(WebPage* page)
    : WebFullScreenManager(page)
{
    m_enterFullScreenListener.adoptNS([[WebFullScreenManagerAnimationListener alloc] initWithManager:this began:&WebFullScreenManagerMac::beganEnterFullScreenAnimation finished:&WebFullScreenManagerMac::finishedEnterFullScreenAnimation]);
    m_exitFullScreenListener.adoptNS([[WebFullScreenManagerAnimationListener alloc] initWithManager:this began:&WebFullScreenManagerMac::beganExitFullScreenAnimation finished:&WebFullScreenManagerMac::finishedExitFullScreenAnimation]);
}

WebFullScreenManagerMac::~WebFullScreenManagerMac()
{
    m_page->send(Messages::WebFullScreenManagerProxy::ExitAcceleratedCompositingMode());
    [m_enterFullScreenListener.get() invalidate];
    [m_exitFullScreenListener.get() invalidate];
}

void WebFullScreenManagerMac::setRootFullScreenLayer(WebCore::GraphicsLayer* layer)
{
    if (m_fullScreenRootLayer == layer)
        return;
    m_fullScreenRootLayer = layer;

    if (!m_fullScreenRootLayer) {
        m_page->send(Messages::WebFullScreenManagerProxy::ExitAcceleratedCompositingMode());
        if (m_rootLayer) {
            m_rootLayer->removeAllChildren();
            m_rootLayer = 0;
        }
        return;
    }

    if (!m_rootLayer) {
        mach_port_t serverPort = WebProcess::shared().compositingRenderServerPort();
        m_remoteLayerClient = WKCARemoteLayerClientMakeWithServerPort(serverPort);

        m_rootLayer = GraphicsLayer::create(NULL);
#ifndef NDEBUG
        m_rootLayer->setName("Full screen root layer");
#endif
        m_rootLayer->setDrawsContent(false);
        m_rootLayer->setSize(getFullScreenRect().size());

        [m_rootLayer->platformLayer() setGeometryFlipped:YES];
        WKCARemoteLayerClientSetLayer(m_remoteLayerClient.get(), m_rootLayer->platformLayer());
        m_layerTreeContext.contextID = WKCARemoteLayerClientGetClientId(m_remoteLayerClient.get());
        m_page->send(Messages::WebFullScreenManagerProxy::EnterAcceleratedCompositingMode(m_layerTreeContext));
    }

    m_rootLayer->removeAllChildren();

    if (m_fullScreenRootLayer)
        m_rootLayer->addChild(m_fullScreenRootLayer);

    m_rootLayer->syncCompositingStateForThisLayerOnly();
    m_page->corePage()->mainFrame()->view()->syncCompositingStateIncludingSubframes();
}

void WebFullScreenManagerMac::beginEnterFullScreenAnimation(float duration)
{
    ASSERT(m_element);
    ASSERT(m_fullScreenRootLayer);

    IntRect destinationFrame = getFullScreenRect();
    m_element->document()->setFullScreenRendererSize(destinationFrame.size());
    m_rootLayer->syncCompositingStateForThisLayerOnly();
    m_page->corePage()->mainFrame()->view()->syncCompositingStateIncludingSubframes();

    // FIXME: Once we gain the ability to do native WebKit animations of generated
    // content, this can change to use them.  Meanwhile, we'll have to animate the
    // CALayer directly:
    CALayer* caLayer = m_fullScreenRootLayer->platformLayer();

    // Create a transformation matrix that will transform the renderer layer such that
    // the fullscreen element appears to move from its starting position and size to its
    // final one.
    CGPoint destinationPosition = [caLayer position];
    CGPoint layerAnchor = [caLayer anchorPoint];
    CGPoint initialPosition = CGPointMake(
        m_initialFrame.x() + m_initialFrame.width() * layerAnchor.x,
        m_initialFrame.y() + m_initialFrame.height() * layerAnchor.y);
    CATransform3D shrinkTransform = CATransform3DMakeScale(
        static_cast<CGFloat>(m_initialFrame.width()) / destinationFrame.width(),
        static_cast<CGFloat>(m_initialFrame.height()) / destinationFrame.height(), 1);
    CATransform3D shiftTransform = CATransform3DMakeTranslation(
        initialPosition.x - destinationPosition.x,
        // Drawing is flipped here, and so much be the translation transformation
        destinationPosition.y - initialPosition.y, 0);
    CATransform3D finalTransform = CATransform3DConcat(shrinkTransform, shiftTransform);

    // Use a CABasicAnimation here for the zoom effect. We want to be notified that the animation has
    // completed by way of the CAAnimation delegate.
    CABasicAnimation* zoomAnimation = [CABasicAnimation animationWithKeyPath:@"transform"];
    [zoomAnimation setFromValue:[NSValue valueWithCATransform3D:finalTransform]];
    [zoomAnimation setToValue:[NSValue valueWithCATransform3D:CATransform3DIdentity]];
    [zoomAnimation setDelegate:m_enterFullScreenListener.get()];
    [zoomAnimation setDuration:duration];
    [zoomAnimation setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut]];
    [zoomAnimation setFillMode:kCAFillModeForwards];

    // Disable implicit animations and set the layer's transformation matrix to its final state.
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [caLayer addAnimation:zoomAnimation forKey:@"zoom"];
    [CATransaction commit];
}

void WebFullScreenManagerMac::beginExitFullScreenAnimation(float duration)
{
    ASSERT(m_element);
    ASSERT(m_fullScreenRootLayer);

    IntRect destinationFrame = getFullScreenRect();
    m_element->document()->setFullScreenRendererSize(destinationFrame.size());
    m_rootLayer->syncCompositingStateForThisLayerOnly();
    m_page->corePage()->mainFrame()->view()->syncCompositingStateIncludingSubframes();

    // FIXME: Once we gain the ability to do native WebKit animations of generated
    // content, this can change to use them.  Meanwhile, we'll have to animate the
    // CALayer directly:
    CALayer* caLayer = m_fullScreenRootLayer->platformLayer();

    // Create a transformation matrix that will transform the renderer layer such that
    // the fullscreen element appears to move from its starting position and size to its
    // final one.
    CGPoint destinationPosition = [(CALayer*)[caLayer presentationLayer] position];
    CGRect destinationBounds = NSRectToCGRect([[caLayer presentationLayer] bounds]);
    CGPoint layerAnchor = [caLayer anchorPoint];
    CGPoint initialPosition = CGPointMake(
        m_initialFrame.x() + m_initialFrame.width() * layerAnchor.x,
        m_initialFrame.y() + m_initialFrame.height() * layerAnchor.y);
    CATransform3D shrinkTransform = CATransform3DMakeScale(
        static_cast<CGFloat>(m_initialFrame.width()) / destinationBounds.size.width,
        static_cast<CGFloat>(m_initialFrame.height()) / destinationBounds.size.height, 1);
    CATransform3D shiftTransform = CATransform3DMakeTranslation(
        initialPosition.x - destinationPosition.x,
        // Drawing is flipped here, and so must be the translation transformation
        destinationPosition.y - initialPosition.y, 0);
    CATransform3D finalTransform = CATransform3DConcat(shrinkTransform, shiftTransform);

    CATransform3D initialTransform = [(CALayer*)[caLayer presentationLayer] transform];

    // Use a CABasicAnimation here for the zoom effect. We want to be notified that the animation has
    // completed by way of the CAAnimation delegate.
    CABasicAnimation* zoomAnimation = [CABasicAnimation animationWithKeyPath:@"transform"];
    [zoomAnimation setFromValue:[NSValue valueWithCATransform3D:initialTransform]];
    [zoomAnimation setToValue:[NSValue valueWithCATransform3D:finalTransform]];
    [zoomAnimation setDelegate:m_exitFullScreenListener.get()];
    [zoomAnimation setDuration:duration];
    [zoomAnimation setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut]];
    [zoomAnimation setFillMode:kCAFillModeForwards];

    // Disable implicit animations and set the layer's transformation matrix to its final state.
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [caLayer addAnimation:zoomAnimation forKey:@"zoom"];
    [caLayer setTransform:finalTransform];
    [CATransaction commit];
}

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
