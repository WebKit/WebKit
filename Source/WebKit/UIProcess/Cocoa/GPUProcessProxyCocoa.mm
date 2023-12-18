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

#if HAVE(POWERLOG_TASK_MODE_QUERY)
#include <pal/spi/mac/PowerLogSPI.h>
#include <wtf/darwin/WeakLinking.h>

WTF_WEAK_LINK_FORCE_IMPORT(PLQueryRegistered);
#endif

namespace WebKit {

void GPUProcessProxy::platformInitializeGPUProcessParameters(GPUProcessCreationParameters& parameters)
{
    parameters.mobileGestaltExtensionHandle = createMobileGestaltSandboxExtensionIfNeeded();
    parameters.gpuToolsExtensionHandles = createGPUToolsSandboxExtensionHandlesIfNeeded();
    parameters.applicationVisibleName = applicationVisibleName();
#if PLATFORM(MAC)
    if (auto launchServicesExtensionHandle = SandboxExtension::createHandleForMachLookup("com.apple.coreservices.launchservicesd"_s, std::nullopt))
        parameters.launchServicesExtensionHandle = WTFMove(*launchServicesExtensionHandle);
#endif
}

#if HAVE(POWERLOG_TASK_MODE_QUERY)
bool GPUProcessProxy::isPowerLoggingInTaskMode()
{
    CFDictionaryRef dictionary = nullptr;
    if (PLQueryRegistered)
        dictionary = PLQueryRegistered(PLClientIDWebKit, CFSTR("TaskModeQuery"), nullptr);
    if (!dictionary)
        return false;
    CFNumberRef taskModeRef = static_cast<CFNumberRef>(CFDictionaryGetValue(dictionary, CFSTR("Task Mode")));
    if (!taskModeRef)
        return false;
    int taskMode = 0;
    if (!CFNumberGetValue(taskModeRef, kCFNumberIntType, &taskMode))
        return false;
    return !!taskMode;
}

void GPUProcessProxy::enablePowerLogging()
{
    RELEASE_LOG(Sandbox, "GPUProcessProxy::enablePowerLogging()");
    auto handle = SandboxExtension::createHandleForMachLookup("com.apple.powerlog.plxpclogger.xpc"_s, std::nullopt);
    if (!handle)
        return;
    send(Messages::GPUProcess::EnablePowerLogging(WTFMove(*handle)), 0);
}
#endif // HAVE(POWERLOG_TASK_MODE_QUERY)

Vector<SandboxExtension::Handle> GPUProcessProxy::createGPUToolsSandboxExtensionHandlesIfNeeded()
{
    if (!WebProcessProxy::shouldEnableRemoteInspector())
        return { };

    return SandboxExtension::createHandlesForMachLookup({ "com.apple.gputools.service"_s, }, std::nullopt);
}

}

#endif
