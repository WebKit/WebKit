/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "WKPageHostedModelView.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(MODEL_PROCESS)

#import "Logging.h"
#import "RealityKitBridging.h"
#import <CoreRE/CoreRE.h>
#import <UIKit/UIKit.h>
#import <WebKitAdditions/REPtr.h>
#import <pal/spi/cocoa/QuartzCoreSPI.h>
#import <wtf/RetainPtr.h>

#import "WebKitSwiftSoftLink.h"

@implementation WKPageHostedModelView {
    RetainPtr<UIView> _remoteModelView;
    RetainPtr<UIView> _containerView;
    REPtr<REEntityRef> _rootEntity;
    REPtr<REEntityRef> _containerEntity;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    CALayer *portalLayer = self.layer;
    portalLayer.name = @"WebKit:PortalLayer";
    [portalLayer setValue:@YES forKeyPath:@"separatedOptions.updates.clippingPrimitive"];
    [portalLayer setValue:@YES forKeyPath:@"separatedOptions.updates.transform"];
    [portalLayer setValue:@YES forKeyPath:@"separatedOptions.updates.collider"];
    [portalLayer setValue:@YES forKeyPath:@"separatedOptions.updates.mesh"];
    [portalLayer setValue:@YES forKeyPath:@"separatedOptions.updates.material"];
    [portalLayer setValue:@YES forKeyPath:@"separatedOptions.updates.texture"];
    [portalLayer setValue:@YES forKeyPath:@"separatedOptions.isPortal"];
    [portalLayer setSeparatedState:kCALayerSeparatedStateSeparated];

    REPtr<REComponentRef> clientComponent = RECALayerGetCALayerClientComponent(portalLayer);
    if (clientComponent) {
        _rootEntity = REComponentGetEntity(clientComponent.get());
        REEntitySetName(_rootEntity.get(), "WebKit:PageHostedModelViewEntity");
    }

    _containerView = adoptNS([[UIView alloc] initWithFrame:self.bounds]);
    [_containerView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [self addSubview:_containerView.get()];
    CALayer *containerViewLayer = [_containerView layer];
    containerViewLayer.name = @"ModelContainerLayer";
    [containerViewLayer setValue:@NO forKeyPath:@"separatedOptions.updates.transform"];
    containerViewLayer.separatedState = kCALayerSeparatedStateTracked;
    REPtr<REComponentRef> containerViewLayerClientComponent = RECALayerGetCALayerClientComponent(containerViewLayer);
    if (containerViewLayerClientComponent) {
        _containerEntity = REComponentGetEntity(containerViewLayerClientComponent.get());
        REEntitySetName(_containerEntity.get(), "WebKit:ModelContainerEntity");
        REEntitySetParent(_containerEntity.get(), _rootEntity.get());
        REEntitySubtreeAddNetworkComponentRecursive(_containerEntity.get());

        // FIXME: Clipping workaround for rdar://125188888 (blocked by rdar://123516357 -> rdar://124718417).
        // containerEntity is required to add a clipping primitive that is independent from model's rootEntity.
        // Adding the primitive directly to clientComponentEntity has no visual effect.
        constexpr float clippingBoxHalfSize = 500; // meters
        REPtr<REComponentRef> clipComponent = REEntityGetOrAddComponentByClass(_containerEntity.get(), REClippingPrimitiveComponentGetComponentType());
        REClippingPrimitiveComponentSetShouldClipChildren(clipComponent.get(), true);
        REClippingPrimitiveComponentSetShouldClipSelf(clipComponent.get(), true);

        REAABB clipBounds { simd_make_float3(-clippingBoxHalfSize, -clippingBoxHalfSize, -2 * clippingBoxHalfSize),
            simd_make_float3(clippingBoxHalfSize, clippingBoxHalfSize, 0) };
        REClippingPrimitiveComponentClipToBox(clipComponent.get(), clipBounds);

        RENetworkMarkEntityMetadataDirty(_rootEntity.get());
        RENetworkMarkEntityMetadataDirty(_containerEntity.get());
    }

    [self applyBackgroundColor:std::nullopt];

    return self;
}

- (void)dealloc
{
    if (_containerEntity)
        REEntityRemoveFromSceneOrParent(_containerEntity.get());
    if (_rootEntity)
        REEntityRemoveFromSceneOrParent(_rootEntity.get());

    [super dealloc];
}

- (UIView *)remoteModelView
{
    return _remoteModelView.get();
}

- (void)setRemoteModelView:(UIView *)remoteModelView
{
    if (_remoteModelView.get() == remoteModelView)
        return;

    [_remoteModelView removeFromSuperview];

    _remoteModelView = remoteModelView;
    CGRect bounds = [_containerView bounds];
    [_remoteModelView setFrame:bounds];
    [_remoteModelView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
    [_containerView addSubview:_remoteModelView.get()];
}

- (void)setShouldDisablePortal:(BOOL)shouldDisablePortal
{
    if (_shouldDisablePortal == shouldDisablePortal)
        return;

    _shouldDisablePortal = shouldDisablePortal;

    if (_shouldDisablePortal) {
        [self.layer setValue:nil forKeyPath:@"separatedOptions.isPortal"];
        [self.layer setValue:@NO forKeyPath:@"separatedOptions.updates.clippingPrimitive"];
    } else {
        [self.layer setValue:@YES forKeyPath:@"separatedOptions.isPortal"];
        [self.layer setValue:@YES forKeyPath:@"separatedOptions.updates.clippingPrimitive"];
    }
}

- (void)applyBackgroundColor:(std::optional<WebCore::Color>)backgroundColor
{
    if (!backgroundColor || !backgroundColor->isValid()) {
        [self.layer setValue:(__bridge id)CGColorGetConstantColor(kCGColorWhite) forKeyPath:@"separatedOptions.material.clearColor"];
        return;
    }

    [self.layer setValue:(__bridge id)cachedCGColor(*backgroundColor).get() forKeyPath:@"separatedOptions.material.clearColor"];
}

@end

#endif // PLATFORM(IOS_FAMILY) && ENABLE(MODEL_PROCESS)
