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

#import "config.h"
#import "LayerHostingContext.h"

#import <WebKitSystemInterface.h>

#if __has_include(<QuartzCore/CAContext.h>)
#import <QuartzCore/CAContext.h>
#else
@interface CAContext : NSObject
@end

@interface CAContext (Details)
- (void)invalidate;
@property (readonly) uint32_t contextId;
@property (strong) CALayer *layer;
@property CGColorSpaceRef colorSpace;
- (void)setFencePort:(mach_port_t)port;
+ (CAContext *)remoteContextWithOptions:(NSDictionary *)dict;
@end
#endif

extern NSString * const kCAContextIgnoresHitTest;

namespace WebKit {

std::unique_ptr<LayerHostingContext> LayerHostingContext::createForPort(mach_port_t serverPort)
{
    auto layerHostingContext = std::make_unique<LayerHostingContext>();

    layerHostingContext->m_layerHostingMode = LayerHostingMode::InProcess;
    layerHostingContext->m_context = (CAContext *)WKCAContextMakeRemoteWithServerPort(serverPort);

    return layerHostingContext;
}

#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
std::unique_ptr<LayerHostingContext> LayerHostingContext::createForExternalHostingProcess()
{
    auto layerHostingContext = std::make_unique<LayerHostingContext>();
    layerHostingContext->m_layerHostingMode = LayerHostingMode::OutOfProcess;

#if PLATFORM(IOS)
    // Use a very large display ID to ensure that the context is never put on-screen 
    // without being explicitly parented. See <rdar://problem/16089267> for details.
    layerHostingContext->m_context = [CAContext remoteContextWithOptions:@{
        kCAContextIgnoresHitTest : @YES,
        kCAContextDisplayId : @10000 }];
#else
    layerHostingContext->m_context = (CAContext *)WKCAContextMakeRemoteForWindowServer();
#endif
    
    return layerHostingContext;
}
#endif

LayerHostingContext::LayerHostingContext()
{
}

LayerHostingContext::~LayerHostingContext()
{
}

void LayerHostingContext::setRootLayer(CALayer *rootLayer)
{
    [m_context setLayer:rootLayer];
}

CALayer *LayerHostingContext::rootLayer() const
{
    return [m_context layer];
}

uint32_t LayerHostingContext::contextID() const
{
    return [m_context contextId];
}

void LayerHostingContext::invalidate()
{
    [m_context invalidate];
}

void LayerHostingContext::setColorSpace(CGColorSpaceRef colorSpace)
{
    [m_context setColorSpace:colorSpace];
}

CGColorSpaceRef LayerHostingContext::colorSpace() const
{
    return [m_context colorSpace];
}

void LayerHostingContext::setFencePort(mach_port_t fencePort)
{
    [m_context setFencePort:fencePort];
}

} // namespace WebKit
