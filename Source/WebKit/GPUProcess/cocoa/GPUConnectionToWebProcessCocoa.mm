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

#import "SystemStatusSPI.h"
#import <WebCore/LocalizedStrings.h>
#import <WebCore/RegistrableDomain.h>
#import <WebCore/SecurityOrigin.h>
#import <pal/ios/SystemStatusSoftLink.h>

namespace WebKit {

#if ENABLE(MEDIA_STREAM)
bool GPUConnectionToWebProcess::setCaptureAttributionString()
{
#if HAVE(SYSTEM_STATUS)
    if (![PAL::getSTDynamicActivityAttributionPublisherClass() respondsToSelector:@selector(setCurrentAttributionStringWithFormat:auditToken:)])
        return true;

    auto domain = WebCore::RegistrableDomain { m_captureOrigin->data() };
    if (domain.isEmpty())
        return false;

    auto auditToken = gpuProcess().parentProcessConnection()->getAuditToken();
    if (!auditToken)
        return false;

    RetainPtr<NSString> formatString = [NSString stringWithFormat:WEB_UI_STRING("“%@” in “%%@”", "The domain and application using the camera and/or microphone. The first argument is domain, the second is the application name (iOS only)."), (NSString *)domain.string()];

    [PAL::getSTDynamicActivityAttributionPublisherClass() setCurrentAttributionStringWithFormat:formatString.get() auditToken:auditToken.value()];
#endif

    return true;
}
#endif // ENABLE(MEDIA_STREAM)

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
