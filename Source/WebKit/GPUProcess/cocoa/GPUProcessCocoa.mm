/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#pragma once

#import "config.h"
#import "GPUProcess.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#import "GPUConnectionToWebProcess.h"
#import "GPUProcessCreationParameters.h"
#import "Logging.h"
#import "RemoteRenderingBackend.h"
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/cocoa/MetalSPI.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/SpanCocoa.h>

#if PLATFORM(MAC)
#include <pal/spi/cocoa/LaunchServicesSPI.h>
#endif

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebKit {
using namespace WebCore;

#if USE(OS_STATE)

RetainPtr<NSDictionary> GPUProcess::additionalStateForDiagnosticReport() const
{
    auto stateDictionary = adoptNS([[NSMutableDictionary alloc] initWithCapacity:1]);
    if (!m_webProcessConnections.isEmpty()) {
        auto webProcessConnectionInfo = adoptNS([[NSMutableDictionary alloc] initWithCapacity:m_webProcessConnections.size()]);
        for (auto& identifierAndConnection : m_webProcessConnections) {
            auto& [webProcessIdentifier, connection] = identifierAndConnection;
            auto& backendMap = connection->remoteRenderingBackendMap();
            if (backendMap.isEmpty())
                continue;

            auto stateInfo = adoptNS([[NSMutableDictionary alloc] initWithCapacity:backendMap.size()]);
            // FIXME: Log some additional diagnostic state on RemoteRenderingBackend.
            [webProcessConnectionInfo setObject:stateInfo.get() forKey:webProcessIdentifier.loggingString()];
        }

        if ([webProcessConnectionInfo count])
            [stateDictionary setObject:webProcessConnectionInfo.get() forKey:@"RemoteRenderingBackend states"];
    }
    return stateDictionary;
}

#endif // USE(OS_STATE)

#if ENABLE(CFPREFS_DIRECT_MODE)
void GPUProcess::dispatchSimulatedNotificationsForPreferenceChange(const String& key)
{
}
#endif // ENABLE(CFPREFS_DIRECT_MODE)

#if ENABLE(MEDIA_STREAM)
void GPUProcess::ensureAVCaptureServerConnection()
{
    RELEASE_LOG(WebRTC, "GPUProcess::ensureAVCaptureServerConnection: Entering.");
#if HAVE(AVCAPTUREDEVICE) && HAVE(AVSAMPLEBUFFERVIDEOOUTPUT)
    if ([PAL::getAVCaptureDeviceClass() respondsToSelector:@selector(ensureServerConnection)]) {
        RELEASE_LOG(WebRTC, "GPUProcess::ensureAVCaptureServerConnection: Calling [AVCaptureDevice ensureServerConnection]");
        [PAL::getAVCaptureDeviceClass() ensureServerConnection];
    }
#endif
}
#endif

void GPUProcess::platformInitializeGPUProcess(GPUProcessCreationParameters& parameters)
{
#if PLATFORM(MAC)
    auto launchServicesExtension = SandboxExtension::create(WTFMove(parameters.launchServicesExtensionHandle));
    if (launchServicesExtension) {
        bool ok = launchServicesExtension->consume();
        ASSERT_UNUSED(ok, ok);
    }

    // It is important to check in with launch services before setting the process name.
    launchServicesCheckIn();

    // Update process name while holding the Launch Services sandbox extension
    updateProcessName();

    // Close connection to launch services.
#if HAVE(HAVE_LS_SERVER_CONNECTION_STATUS_RELEASE_NOTIFICATIONS_MASK)
    _LSSetApplicationLaunchServicesServerConnectionStatus(kLSServerConnectionStatusDoNotConnectToServerMask | kLSServerConnectionStatusReleaseNotificationsMask, nullptr);
#else
    _LSSetApplicationLaunchServicesServerConnectionStatus(kLSServerConnectionStatusDoNotConnectToServerMask, nullptr);
#endif

    if (launchServicesExtension)
        launchServicesExtension->revoke();
#endif // PLATFORM(MAC)
#if USE(SANDBOX_EXTENSIONS_FOR_CACHE_AND_TEMP_DIRECTORY_ACCESS) && USE(EXTENSIONKIT)
    MTLSetShaderCachePath(parameters.containerCachesDirectory);
#endif
}

#if USE(EXTENSIONKIT)
void GPUProcess::resolveBookmarkDataForCacheDirectory(std::span<const uint8_t> bookmarkData)
{
    RetainPtr bookmark = toNSData(bookmarkData);
    BOOL bookmarkIsStale = NO;
    NSError* error = nil;
    [NSURL URLByResolvingBookmarkData:bookmark.get() options:NSURLBookmarkResolutionWithoutUI relativeToURL:nil bookmarkDataIsStale:&bookmarkIsStale error:&error];
}
#endif

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
