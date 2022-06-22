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

#include "GPUProcessCreationParameters.h"
#include "GPUProcessMessages.h"
#include "MediaPermissionUtilities.h"

#if HAVE(POWERLOG_TASK_MODE_QUERY)
#include <pal/spi/mac/PowerLogSPI.h>
#endif

namespace WebKit {

void GPUProcessProxy::platformInitializeGPUProcessParameters(GPUProcessCreationParameters& parameters)
{
    parameters.mobileGestaltExtensionHandle = createMobileGestaltSandboxExtensionIfNeeded();
    parameters.applicationVisibleName = applicationVisibleName();
}

#if HAVE(POWERLOG_TASK_MODE_QUERY)
bool GPUProcessProxy::isPowerLoggingInTaskMode()
{
    PLClientID clientIDWebKit = static_cast<PLClientID>(132); // FIXME: Replace with PLClientIDWebKit when available in SDK
    auto dictionary = adoptCF(PLQueryRegistered(clientIDWebKit, CFSTR("TaskModeQuery"), nullptr));
    if (!dictionary)
        return false;
    CFNumberRef taskModeRef = static_cast<CFNumberRef>(CFDictionaryGetValue(dictionary.get(), CFSTR("Task Mode")));
    if (!taskModeRef)
        return false;
    int taskMode = 0;
    if (!CFNumberGetValue(taskModeRef, kCFNumberIntType, &taskMode))
        return false;
    return !!taskMode;
}

void GPUProcessProxy::enablePowerLogging()
{
    auto handle = SandboxExtension::createHandleForMachLookup("com.apple.powerlog.plxpclogger.xpc"_s, std::nullopt);
    if (!handle)
        return;
    send(Messages::GPUProcess::EnablePowerLogging(*handle), 0);
}
#endif // HAVE(POWERLOG_TASK_MODE_QUERY)

}

#endif
