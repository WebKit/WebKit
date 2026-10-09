/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "GPUConnectionToWebProcess.h"

#if ENABLE(GPU_PROCESS)

#import "Logging.h"
#import "MediaPermissionUtilities.h"
#import <WebCore/LocalizedStrings.h>
#if ENABLE(UNIFIED_MODEL_RENDERING)
#import "LayerHostingContext.h"
#import <CoreVideo/CVPixelBuffer.h>
#import <IOSurface/IOSurface.h>
#import <Metal/Metal.h>
#import <QuartzCore/CALayer.h>
#import <WebCore/LayerHostingContextIdentifier.h>
#import <wtf/cf/TypeCastsCF.h>
#endif
#import <WebCore/RealtimeMediaSourceCenter.h>
#import <WebCore/RegistrableDomain.h>
#import <WebCore/SecurityOrigin.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <wtf/OSObjectPtr.h>

#if HAVE(SYSTEM_STATUS)
#import "SystemStatusSPI.h"
#import <pal/ios/SystemStatusSoftLink.h>
#endif

#import "TCCSoftLink.h"

namespace WebKit {

#if ENABLE(MEDIA_STREAM)
bool GPUConnectionToWebProcess::setCaptureAttributionString()
{
#if HAVE(SYSTEM_STATUS)
    if (![PAL::getSTDynamicActivityAttributionPublisherClassSingleton() respondsToSelector:@selector(setCurrentAttributionStringWithFormat:auditToken:)]
        && ![PAL::getSTDynamicActivityAttributionPublisherClassSingleton() respondsToSelector:@selector(setCurrentAttributionWebsiteString:auditToken:)]) {
        return true;
    }

    auto auditToken = gpuProcess().parentProcessConnection()->getAuditToken();
    if (!auditToken)
        return false;

    RetainPtr visibleName = applicationVisibleNameFromOrigin(m_captureOrigin->data());
    if (!visibleName)
        visibleName = gpuProcess().applicationVisibleName().createNSString();

    if ([PAL::getSTDynamicActivityAttributionPublisherClassSingleton() respondsToSelector:@selector(setCurrentAttributionWebsiteString:auditToken:)])
        [PAL::getSTDynamicActivityAttributionPublisherClassSingleton() setCurrentAttributionWebsiteString:visibleName.get() auditToken:auditToken.value()];
    else {
        SUPPRESS_UNRETAINED_ARG RetainPtr formatString = adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"%@ in %%@", "The domain and application using the camera and/or microphone. The first argument is domain, the second is the application name (iOS only)."), visibleName.get()]);
        [PAL::getSTDynamicActivityAttributionPublisherClassSingleton() setCurrentAttributionStringWithFormat:formatString.get() auditToken:auditToken.value()];
    }
#endif

    return true;
}
#endif // ENABLE(MEDIA_STREAM)

#if ENABLE(APP_PRIVACY_REPORT)
void GPUConnectionToWebProcess::setTCCIdentity()
{
#if !PLATFORM(MACCATALYST)
    auto auditToken = protect(gpuProcess().parentProcessConnection())->getAuditToken();
    if (!auditToken) {
        RELEASE_LOG_ERROR(WebRTC, "getAuditToken returned null");
        return;
    }

    NSError *error = nil;
    auto bundleProxy = [LSBundleProxy bundleProxyWithAuditToken:*auditToken error:&error];
    RELEASE_LOG_ERROR_IF(error, WebRTC, "-[LSBundleProxy bundleProxyWithAuditToken:error:] failed with error %s", [[error localizedDescription] UTF8String]);

    String bundleIdentifier = bundleProxy.bundleIdentifier;
    if (bundleIdentifier.isNull())
        bundleIdentifier = m_applicationBundleIdentifier;

    if (bundleIdentifier.isNull()) {
        RELEASE_LOG_ERROR(WebRTC, "Unable to get the bundle identifier");
        return;
    }

    // FIXME: Adopting is needed here but static analysis is not able to tell.
    SUPPRESS_RETAINPTR_CTOR_ADOPT OSObjectPtr identity = adoptOSObject(tcc_identity_create(TCC_IDENTITY_CODE_BUNDLE_ID, bundleIdentifier.utf8().legacyCStringPointer()));
    if (!identity) {
        RELEASE_LOG_ERROR(WebRTC, "tcc_identity_create returned null");
        return;
    }

    WebCore::RealtimeMediaSourceCenter::singleton().setIdentity(WTF::move(identity));
#endif // !PLATFORM(MACCATALYST)
}
#endif // ENABLE(APP_PRIVACY_REPORT)

#if ENABLE(EXTENSION_CAPABILITIES)
String GPUConnectionToWebProcess::mediaPlaybackEnvironment(WebCore::PageIdentifier pageIdentifier)
{
    return m_mediaPlaybackEnvironments.get(pageIdentifier);
}

void GPUConnectionToWebProcess::setMediaPlaybackEnvironment(WebCore::PageIdentifier pageIdentifier, const String& mediaPlaybackEnvironment)
{
    if (mediaPlaybackEnvironment.isEmpty())
        m_mediaPlaybackEnvironments.remove(pageIdentifier);
    else
        m_mediaPlaybackEnvironments.set(pageIdentifier, mediaPlaybackEnvironment);
}

String GPUConnectionToWebProcess::displayCaptureEnvironment(WebCore::PageIdentifier pageIdentifier)
{
    return m_displayCaptureEnvironments.get(pageIdentifier);
}

void GPUConnectionToWebProcess::setDisplayCaptureEnvironment(WebCore::PageIdentifier pageIdentifier, const String& displayCaptureEnvironment)
{
    if (displayCaptureEnvironment.isEmpty())
        m_displayCaptureEnvironments.remove(pageIdentifier);
    else
        m_displayCaptureEnvironments.set(pageIdentifier, displayCaptureEnvironment);
}
#endif

#if ENABLE(UNIFIED_MODEL_RENDERING)
void GPUConnectionToWebProcess::createModelLayerHostingContext(CompletionHandler<void(std::optional<WebCore::LayerHostingContextIdentifier>)>&& completionHandler)
{
    constexpr size_t width = 512;
    constexpr size_t height = 512;

    RetainPtr<IOSurfaceRef> surface = adoptCF(IOSurfaceCreate((__bridge CFDictionaryRef)@{
        (__bridge id)kIOSurfaceWidth: @(width),
        (__bridge id)kIOSurfaceHeight: @(height),
        (__bridge id)kIOSurfaceBytesPerElement: @4,
        (__bridge id)kIOSurfacePixelFormat: @(kCVPixelFormatType_32BGRA),
    }));
    if (!surface) {
        completionHandler(std::nullopt);
        return;
    }

    if (RetainPtr<id<MTLDevice>> device = adoptNS(MTLCreateSystemDefaultDevice())) {
        RetainPtr<MTLTextureDescriptor> desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:width height:height mipmapped:NO];
        [desc.get() setUsage:MTLTextureUsageRenderTarget];
        [desc.get() setStorageMode:MTLStorageModeShared];
        RetainPtr<id<MTLTexture>> texture = adoptNS([device.get() newTextureWithDescriptor:desc.get() iosurface:surface.get() plane:0]);
        RetainPtr<id<MTLCommandQueue>> queue = adoptNS([device.get() newCommandQueue]);
        RetainPtr<MTLRenderPassDescriptor> pass = [MTLRenderPassDescriptor renderPassDescriptor];
        RetainPtr<MTLRenderPassColorAttachmentDescriptor> colorAttachment = [[pass.get() colorAttachments] objectAtIndexedSubscript:0];
        [colorAttachment.get() setTexture:texture.get()];
        [colorAttachment.get() setLoadAction:MTLLoadActionClear];
        [colorAttachment.get() setStoreAction:MTLStoreActionStore];
        [colorAttachment.get() setClearColor:MTLClearColorMake(0, 1, 0, 1)];
        RetainPtr<id<MTLCommandBuffer>> commandBuffer = [queue.get() commandBuffer];
        RetainPtr<id<MTLRenderCommandEncoder>> encoder = [commandBuffer.get() renderCommandEncoderWithDescriptor:pass.get()];
        [encoder.get() endEncoding];
        [commandBuffer.get() commit];
        [commandBuffer.get() waitUntilCompleted];
    }

    m_modelLayerHostingContext = LayerHostingContext::create();

    RetainPtr<CALayer> layer = adoptNS([[CALayer alloc] init]);
    [layer setName:@"WebKit:GPUProcessModelIOSurfaceLayer"];
    [layer setFrame:CGRectMake(0, 0, width, height)];
    [layer setContents:(__bridge id)surface.get()];
    m_modelLayerHostingContext->setRootLayer(layer.get());

    completionHandler(WebCore::LayerHostingContextIdentifier(m_modelLayerHostingContext->contextID()));
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
