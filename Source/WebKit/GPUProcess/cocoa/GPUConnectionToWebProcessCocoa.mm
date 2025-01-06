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
    if (![PAL::getSTDynamicActivityAttributionPublisherClass() respondsToSelector:@selector(setCurrentAttributionStringWithFormat:auditToken:)]
        && ![PAL::getSTDynamicActivityAttributionPublisherClass() respondsToSelector:@selector(setCurrentAttributionWebsiteString:auditToken:)]) {
        return true;
    }

    auto auditToken = gpuProcess().parentProcessConnection()->getAuditToken();
    if (!auditToken)
        return false;

    auto *visibleName = applicationVisibleNameFromOrigin(m_captureOrigin->data());
    if (!visibleName)
        visibleName = gpuProcess().applicationVisibleName();

    if ([PAL::getSTDynamicActivityAttributionPublisherClass() respondsToSelector:@selector(setCurrentAttributionWebsiteString:auditToken:)])
        [PAL::getSTDynamicActivityAttributionPublisherClass() setCurrentAttributionWebsiteString:visibleName auditToken:auditToken.value()];
    else {
        RetainPtr<NSString> formatString = [NSString stringWithFormat:WEB_UI_NSSTRING(@"%@ in %%@", "The domain and application using the camera and/or microphone. The first argument is domain, the second is the application name (iOS only)."), visibleName];
        [PAL::getSTDynamicActivityAttributionPublisherClass() setCurrentAttributionStringWithFormat:formatString.get() auditToken:auditToken.value()];
    }
#endif

    return true;
}
#endif // ENABLE(MEDIA_STREAM)

#if ENABLE(APP_PRIVACY_REPORT)
void GPUConnectionToWebProcess::setTCCIdentity()
{
#if !PLATFORM(MACCATALYST)
    auto auditToken = gpuProcess().parentProcessConnection()->getAuditToken();
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

    auto identity = adoptOSObject(tcc_identity_create(TCC_IDENTITY_CODE_BUNDLE_ID, bundleIdentifier.utf8().data()));
    if (!identity) {
        RELEASE_LOG_ERROR(WebRTC, "tcc_identity_create returned null");
        return;
    }

    WebCore::RealtimeMediaSourceCenter::singleton().setIdentity(WTFMove(identity));
#endif // !PLATFORM(MACCATALYST)
}
#endif // ENABLE(APP_PRIVACY_REPORT)

#if ENABLE(EXTENSION_CAPABILITIES)
String GPUConnectionToWebProcess::mediaEnvironment(WebCore::PageIdentifier pageIdentifier)
{
    return m_mediaEnvironments.get(pageIdentifier);
}

void GPUConnectionToWebProcess::setMediaEnvironment(WebCore::PageIdentifier pageIdentifier, const String& mediaEnvironment)
{
    if (mediaEnvironment.isEmpty())
        m_mediaEnvironments.remove(pageIdentifier);
    else
        m_mediaEnvironments.set(pageIdentifier, mediaEnvironment);
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
