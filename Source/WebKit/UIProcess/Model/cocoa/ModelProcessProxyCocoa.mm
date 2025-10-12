/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ModelProcessProxy.h"

#if PLATFORM(VISION) && ENABLE(GPU_PROCESS) && ENABLE(MODEL_PROCESS)

#include "GPUProcessProxy.h"
#include "ModelProcessCreationParameters.h"
#include "RunningBoardServicesSPI.h"
#include "SharedFileHandle.h"
#include "SharedPreferencesForWebProcess.h"
#include "WebProcessProxy.h"

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, connection())

namespace WebKit {

void ModelProcessProxy::updateModelProcessCreationParameters(ModelProcessCreationParameters& parameters)
{
    NSString * const debugFlag = @"ModelProcessDebugEnableRestrictiveRenderingMode";
    bool enableRestrictiveRenderingMode = true;
    id value = [NSUserDefaults.standardUserDefaults objectForKey:debugFlag];
    if ([value isKindOfClass:NSNumber.class] || [value isKindOfClass:NSString.class])
        enableRestrictiveRenderingMode = [NSUserDefaults.standardUserDefaults boolForKey:debugFlag];
    parameters.restrictiveRenderingMode = enableRestrictiveRenderingMode;

    NSString * const memoryLimitFlag = @"WebKitDebugModelEntityMemoryLimit";
    value = [NSUserDefaults.standardUserDefaults objectForKey:memoryLimitFlag];
    if ([value isKindOfClass:NSNumber.class])
        parameters.debugEntityMemoryLimit = [value integerValue];
}

void ModelProcessProxy::requestSharedSimulationConnection(WebCore::ProcessIdentifier webProcessIdentifier, CompletionHandler<void(std::optional<IPC::SharedFileHandle>)>&& completionHandler)
{
    auto webProcessProxy = WebProcessProxy::processForIdentifier(webProcessIdentifier);
    if (!webProcessProxy) {
        RELEASE_LOG_ERROR(Process, "%p - ModelProcessProxy::requestSharedSimulationConnection() No WebProcessProxy with this identifier", this);
        completionHandler(std::nullopt);
        return;
    }

    MESSAGE_CHECK(webProcessProxy->sharedPreferencesForWebProcessValue().modelElementEnabled);
    MESSAGE_CHECK(webProcessProxy->sharedPreferencesForWebProcessValue().modelProcessEnabled);
    // We only expect shared simulation connection to be set up once for each model process instance.
    MESSAGE_CHECK(!m_didInitializeSharedSimulationConnection);

    m_didInitializeSharedSimulationConnection = true;

    NSError *error;
    RBSProcessHandle *process = [RBSProcessHandle handleForIdentifier:[RBSProcessIdentifier identifierWithPid:processID()] error:&error];
    if (error) {
        RELEASE_LOG_ERROR(ModelElement, "%p - ModelProcessProxy: Failed to get audit token for requesting process: %@", this, error);
        completionHandler(std::nullopt);
        return;
    }

    RELEASE_LOG(ModelElement, "%p - ModelProcessProxy: Requesting shared simulation connection for model process with audit token for pid=%d", this, processID());
    GPUProcessProxy::getOrCreate()->requestSharedSimulationConnection(process.auditToken, WTFMove(completionHandler));
}

}

#undef MESSAGE_CHECK

#endif

