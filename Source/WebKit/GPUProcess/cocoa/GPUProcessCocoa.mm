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

#pragma once

#import "config.h"
#import "GPUProcess.h"

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)

#import "GPUConnectionToWebProcess.h"
#import "RemoteRenderingBackend.h"
#import <wtf/RetainPtr.h>

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
void GPUProcess::notifyPreferencesChanged(const String& domain, const String& key, const std::optional<String>& encodedValue)
{
    preferenceDidUpdate(domain, key, encodedValue);
}

void GPUProcess::dispatchSimulatedNotificationsForPreferenceChange(const String& key)
{
}

#endif // ENABLE(CFPREFS_DIRECT_MODE)

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
