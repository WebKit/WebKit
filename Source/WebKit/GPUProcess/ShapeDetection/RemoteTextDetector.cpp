/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "RemoteTextDetector.h"

#if ENABLE(GPU_PROCESS)

#include "ArgumentCoders.h"
#include "RemoteRenderingBackend.h"
#include "SharedPreferencesForWebProcess.h"
#include <WebCore/DetectedTextInterface.h>
#include <WebCore/NativeImage.h>
#include <WebCore/TextDetectorInterface.h>
#include <wtf/TZoneMallocInlines.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, m_renderingBackend.get().streamConnection());

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteTextDetector);

RemoteTextDetector::RemoteTextDetector(Ref<WebCore::ShapeDetection::TextDetector>&& textDetector, RemoteRenderingBackend& renderingBackend, ShapeDetectionIdentifier identifier)
    : m_backing(WTF::move(textDetector))
    , m_renderingBackend(renderingBackend)
    , m_identifier(identifier)
{
}

RemoteTextDetector::~RemoteTextDetector() = default;

std::optional<SharedPreferencesForWebProcess> RemoteTextDetector::sharedPreferencesForWebProcess() const
{
    return Ref { m_renderingBackend.get() }->sharedPreferencesForWebProcess();
}

void RemoteTextDetector::detect(WebCore::RenderingResourceIdentifier renderingResourceIdentifier, CompletionHandler<void(Vector<WebCore::ShapeDetection::DetectedText>&&)>&& completionHandler)
{
    RefPtr sourceImage = m_renderingBackend.get().remoteResourceCache().cachedNativeImage(renderingResourceIdentifier);
    MESSAGE_CHECK(sourceImage);
    m_backing->detect(*sourceImage, WTF::move(completionHandler));
}

} // namespace WebKit

#undef MESSAGE_CHECK

#endif // ENABLE(GPU_PROCESS)
