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

#import <WebCore/SecurityOrigin.h>

namespace WebKit {

#if ENABLE(MEDIA_STREAM)
bool GPUConnectionToWebProcess::setCaptureAttributionString()
{
    return gpuProcess().setCaptureAttributionString(m_captureOrigin->data());
}
#endif

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

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
