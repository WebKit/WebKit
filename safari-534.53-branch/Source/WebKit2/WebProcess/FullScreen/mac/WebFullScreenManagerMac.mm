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
#import <WebCore/RenderLayer.h>
#import <WebCore/RenderLayerBacking.h>
#import <WebCore/RenderObject.h>
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
    if (!m_rootLayer && !layer)
        return;

    if (!layer) {
        PlatformLayer* rootPlatformLayer = m_rootLayer->platformLayer();
        [[NSNotificationCenter defaultCenter] postNotificationName:@"WebKitLayerHostChanged" object:rootPlatformLayer userInfo:nil];
        m_rootLayer->removeAllChildren();
        m_rootLayer->syncCompositingStateForThisLayerOnly();
        m_rootLayer = nullptr;

        m_page->forceRepaintWithoutCallback();
        m_page->send(Messages::WebFullScreenManagerProxy::ExitAcceleratedCompositingMode());
        return;
    }

    if (m_rootLayer && m_rootLayer->children().contains(layer))
        return;

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

    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    m_rootLayer->removeAllChildren();
    m_rootLayer->addChild(layer);
    m_rootLayer->syncCompositingState();
    [CATransaction commit];

    [[NSNotificationCenter defaultCenter] postNotificationName:@"WebKitLayerHostChanged" object:m_rootLayer->platformLayer() userInfo:nil];
}

void WebFullScreenManagerMac::disposeOfLayerClient()
{
    if (!m_remoteLayerClient)
        return;
    WKCARemoteLayerClientSetLayer(m_remoteLayerClient.get(), 0);
    WKCARemoteLayerClientInvalidate(m_remoteLayerClient.get());
    m_remoteLayerClient = nullptr;
}

void WebFullScreenManagerMac::beginEnterFullScreenAnimation(float duration)
{
    ASSERT(m_element);
    
    if (!m_rootLayer || m_rootLayer->children().isEmpty()) {
        // If we don't have a root layer, we can't animate in and out of full screen
        m_page->send(Messages::WebFullScreenManagerProxy::EnterAcceleratedCompositingMode(m_layerTreeContext));
        this->beganEnterFullScreenAnimation();
        this->finishedEnterFullScreenAnimation(true);
        m_page->send(Messages::WebFullScreenManagerProxy::ExitAcceleratedCompositingMode());
        return;
    }

    IntRect destinationFrame = getFullScreenRect();
    m_element->document()->setFullScreenRendererSize(destinationFrame.size());
    m_rootLayer->syncCompositingState();

    // FIXME: Once we gain the ability to do native WebKit animations of generated
    // content, this can change to use them.  Meanwhile, we'll have to animate the
    // CALayer directly.
    animateFullScreen(windowedCGTransform(), CATransform3DIdentity, duration, m_enterFullScreenListener.get());
}

void WebFullScreenManagerMac::beginExitFullScreenAnimation(float duration)
{
    ASSERT(m_element);
    
    if (!m_rootLayer || m_rootLayer->children().isEmpty()) {
        // If we don't have a root layer, we can't animate in and out of full screen
        m_page->send(Messages::WebFullScreenManagerProxy::EnterAcceleratedCompositingMode(m_layerTreeContext));
        this->beganExitFullScreenAnimation();
        this->finishedExitFullScreenAnimation(true);
        m_page->send(Messages::WebFullScreenManagerProxy::ExitAcceleratedCompositingMode());
        return;
    }

    IntRect destinationFrame = getFullScreenRect();
    m_element->document()->setFullScreenRendererSize(destinationFrame.size());
    m_rootLayer->syncCompositingState();

    // FIXME: Once we gain the ability to do native WebKit animations of generated
    // content, this can change to use them.  Meanwhile, we'll have to animate the
    // CALayer directly.
    CALayer* caLayer = m_rootLayer->children().first()->platformLayer();
    CALayer* presentationLayer = [caLayer presentationLayer] ? (CALayer*)[caLayer presentationLayer] : caLayer;
    animateFullScreen([presentationLayer transform], windowedCGTransform(), duration, m_exitFullScreenListener.get());
}

void WebFullScreenManagerMac::animateFullScreen(const CATransform3D& startTransform, const CATransform3D& endTransform, float duration, id listener)
{
    // This is the full size of the screen.
    IntRect fullScreenRect = getFullScreenRect();    
    CALayer* caLayer = m_rootLayer->children().first()->platformLayer();

    // This animation represents the zoom effect.
    CABasicAnimation* zoomAnimation = [CABasicAnimation animationWithKeyPath:@"transform"];
    [zoomAnimation setFromValue:[NSValue valueWithCATransform3D:startTransform]];
    [zoomAnimation setToValue:[NSValue valueWithCATransform3D:endTransform]];
    
    // This animation provides a position correction to the CALayer, which might be offset based on the page's
    // scroll position. We animate it instead of just setting the property because others might try to
    // alter its position while the animation is playing.
    CGPoint layerAnchor = [caLayer anchorPoint];
    CGPoint fullScreenPosition = CGPointMake(
        fullScreenRect.x() + fullScreenRect.width() * layerAnchor.x,
        fullScreenRect.y() + fullScreenRect.height() * layerAnchor.y);
    CABasicAnimation* positionCorrection = [CABasicAnimation animationWithKeyPath:@"position"];
    [positionCorrection setFromValue:[NSValue valueWithPoint:NSPointFromCGPoint(fullScreenPosition)]];
    [positionCorrection setToValue:[NSValue valueWithPoint:NSPointFromCGPoint(fullScreenPosition)]];
    
    // We want to be notified that the animation has completed by way of the CAAnimation delegate.
    CAAnimationGroup* transitionAnimation = [CAAnimationGroup animation];
    [transitionAnimation setAnimations:[NSArray arrayWithObjects:zoomAnimation, positionCorrection, nil]];
    [transitionAnimation setDelegate:listener];
    [transitionAnimation setDuration:duration];
    [transitionAnimation setTimingFunction:[CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseInEaseOut]];
    [transitionAnimation setFillMode:kCAFillModeForwards];
    [transitionAnimation setRemovedOnCompletion:NO];
    
    // Disable implicit animations and set the layer's transformation matrix to its final state.
    [CATransaction begin];
    [CATransaction setDisableActions:YES];
    [caLayer addAnimation:transitionAnimation forKey:@"zoom"];
    [CATransaction commit];
}

CATransform3D WebFullScreenManagerMac::windowedCGTransform()
{
    IntRect fullScreenRect = getFullScreenRect();
    RenderLayer* layer = m_element->renderer() ? m_element->renderer()->enclosingLayer() : 0;
    RenderLayerBacking* backing = layer ? layer->backing() : 0;
    IntSize fullScreenSize = fullScreenRect.size();
    if (backing)
        fullScreenSize = backing->contentsBox().size();

    CALayer* caLayer = m_rootLayer->children().first()->platformLayer();

    // Create a transformation matrix that will transform the renderer layer such that
    // the fullscreen element appears to move from its starting position and size to its
    // final one.
    CGPoint layerAnchor = [caLayer anchorPoint];
    CGPoint fullScreenPosition = CGPointMake(
        fullScreenRect.x() + fullScreenRect.width() * layerAnchor.x,
        fullScreenRect.y() + fullScreenRect.height() * layerAnchor.y); //[presentationLayer position];
    CGPoint windowedPosition = CGPointMake(
        m_initialFrame.x() + m_initialFrame.width() * layerAnchor.x,
        m_initialFrame.y() + m_initialFrame.height() * layerAnchor.y);
    CATransform3D shrinkTransform = CATransform3DMakeScale(
        static_cast<CGFloat>(m_initialFrame.width()) / fullScreenSize.width(),
        static_cast<CGFloat>(m_initialFrame.height()) / fullScreenSize.height(), 1);
    CATransform3D shiftTransform = CATransform3DMakeTranslation(
        windowedPosition.x - fullScreenPosition.x,
        // Drawing is flipped here, and so must be the translation transformation
        fullScreenPosition.y - windowedPosition.y, 0);
    
    return CATransform3DConcat(shrinkTransform, shiftTransform);
}

} // namespace WebKit

#endif // ENABLE(FULLSCREEN_API)
