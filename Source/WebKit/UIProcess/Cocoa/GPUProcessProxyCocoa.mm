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

#include "config.h"
#include "GPUProcessProxy.h"

#if ENABLE(GPU_PROCESS)

#include "ArgumentCodersCocoa.h"
#include "GPUProcessCreationParameters.h"
#include "GPUProcessMessages.h"
#include "MediaPermissionUtilities.h"
#include "WebProcessProxy.h"
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/cocoa/SpanCocoa.h>
#include <wtf/darwin/DispatchExtras.h>

#if HAVE(POWERLOG_TASK_MODE_QUERY)
#include <pal/spi/mac/PowerLogSPI.h>
#include <wtf/darwin/WeakLinking.h>

WTF_WEAK_LINK_FORCE_IMPORT(PLQueryRegistered);
#endif

namespace WebKit {

bool GPUProcessProxy::s_enableMetalDebugDeviceInNewGPUProcessesForTesting { false };
bool GPUProcessProxy::s_enableMetalShaderValidationInNewGPUProcessesForTesting { false };

void GPUProcessProxy::platformInitializeGPUProcessParameters(GPUProcessCreationParameters& parameters)
{
    parameters.mobileGestaltExtensionHandle = createMobileGestaltSandboxExtensionIfNeeded();
    parameters.gpuToolsExtensionHandles = createGPUToolsSandboxExtensionHandlesIfNeeded();
    parameters.applicationVisibleName = applicationVisibleName().get();
#if PLATFORM(MAC)
    if (auto launchServicesExtensionHandle = SandboxExtension::createHandleForMachLookup("com.apple.coreservices.launchservicesd"_s, std::nullopt))
        parameters.launchServicesExtensionHandle = WTF::move(*launchServicesExtensionHandle);
#endif
    parameters.enableMetalDebugDeviceForTesting = m_isMetalDebugDeviceEnabledForTesting;
    parameters.enableMetalShaderValidationForTesting = m_isMetalShaderValidationEnabledForTesting;
#if PLATFORM(MAC)
    parameters.processStartupSandboxExtensions.createSandboxExtensions("com.apple.WebKit.GPU"_s);
#endif
}

#if HAVE(POWERLOG_TASK_MODE_QUERY)
bool GPUProcessProxy::isPowerLoggingInTaskMode()
{
    RetainPtr<CFDictionaryRef> dictionary;
    if (PLQueryRegistered)
        dictionary = PLQueryRegistered(PLClientIDWebKit, CFSTR("TaskModeQuery"), nullptr);
    if (!dictionary)
        return false;

    RetainPtr taskModeRef = CFDictionaryGetValue(dictionary.get(), CFSTR("Task Mode"));
    if (!taskModeRef)
        return false;

    if (RetainPtr booleanRef = dynamic_cf_cast<CFBooleanRef>(taskModeRef.get()))
        return !!CFBooleanGetValue(booleanRef.get());

    RetainPtr numberRef = dynamic_cf_cast<CFNumberRef>(taskModeRef.get());
    if (!numberRef) {
        RELEASE_LOG_ERROR(Sandbox, "GPUProcessProxy::isPowerLoggingInTaskMode: Could not process task mode as a boolean or number");
        return false;
    }

    int taskMode = 0;
    if (!CFNumberGetValue(numberRef.get(), kCFNumberIntType, &taskMode))
        return false;
    return !!taskMode;
}

void GPUProcessProxy::enablePowerLogging()
{
    RELEASE_LOG(Sandbox, "GPUProcessProxy::enablePowerLogging()");
    auto handle = SandboxExtension::createHandleForMachLookup("com.apple.powerlog.plxpclogger.xpc"_s, std::nullopt);
    if (!handle)
        return;
    send(Messages::GPUProcess::EnablePowerLogging(WTF::move(*handle)), 0);
}
#endif // HAVE(POWERLOG_TASK_MODE_QUERY)

Vector<SandboxExtension::Handle> GPUProcessProxy::createGPUToolsSandboxExtensionHandlesIfNeeded()
{
    if (!WebProcessProxy::shouldEnableRemoteInspector())
        return { };

    return SandboxExtension::createHandlesForMachLookup({ "com.apple.gputools.service"_s, }, std::nullopt);
}

#if USE(EXTENSIONKIT)
void GPUProcessProxy::sendBookmarkDataForCacheDirectory()
{
    Ref protectedConnection = connection();
    dispatch_async(globalDispatchQueueSingleton(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), makeBlockPtr([protectedConnection = WTF::move(protectedConnection)] () mutable {
        NSError *error = nil;
        RetainPtr directoryURL = [[NSFileManager defaultManager] URLForDirectory:NSLibraryDirectory inDomain:NSUserDomainMask appropriateForURL:nil create:NO error:&error];
        RetainPtr url = adoptNS([[NSURL alloc] initFileURLWithPath:@"Caches/com.apple.WebKit.GPU/" relativeToURL:directoryURL.get()]);
        error = nil;
        RetainPtr bookmark = [url bookmarkDataWithOptions:NSURLBookmarkCreationMinimalBookmark includingResourceValuesForKeys:nil relativeToURL:nil error:&error];
        protectedConnection->send(Messages::GPUProcess::ResolveBookmarkDataForCacheDirectory(span(bookmark.get())), 0);
    }).get());
}
#endif

void GPUProcessProxy::postWillTakeSnapshotNotification(CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::GPUProcess::PostWillTakeSnapshotNotification { }, WTF::move(completionHandler));
}

void GPUProcessProxy::sinkCompletedSnapshotToPDF(RemoteSnapshotIdentifier identifier, const WebCore::FloatSize& size, WebCore::FrameIdentifier rootFrameIdentifier, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& completionHandler)
{
    sendWithAsyncReply(Messages::GPUProcess::SinkCompletedSnapshotToPDF { identifier, size, rootFrameIdentifier }, WTF::move(completionHandler));
}

}

#endif
