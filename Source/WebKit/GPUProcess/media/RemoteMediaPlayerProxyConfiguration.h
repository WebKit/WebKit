/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include <WebCore/ContentType.h>
#include <WebCore/FourCC.h>
#include <WebCore/LayoutRect.h>
#include <WebCore/PlatformTextTrack.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

struct RemoteMediaPlayerProxyConfiguration {
    String referrer;
    String userAgent;
    String sourceApplicationIdentifier;
    String networkInterfaceName;
    Vector<WebCore::ContentType> mediaContentTypesRequiringHardwareSupport;
    std::optional<Vector<String>> allowedMediaContainerTypes;
    std::optional<Vector<String>> allowedMediaCodecTypes;
    std::optional<Vector<WebCore::FourCC>> allowedMediaVideoCodecIDs;
    std::optional<Vector<WebCore::FourCC>> allowedMediaAudioCodecIDs;
    std::optional<Vector<WebCore::FourCC>> allowedMediaCaptionFormatTypes;
    WebCore::LayoutRect playerContentBoxRect;
    Vector<String> preferredAudioCharacteristics;
#if ENABLE(AVF_CAPTIONS)
    Vector<WebCore::PlatformTextTrackData> outOfBandTrackData;
#endif
    WebCore::SecurityOriginData documentSecurityOrigin;
    WebCore::IntSize presentationSize { };
    WebCore::FloatSize videoInlineSize { };
    uint64_t logIdentifier { 0 };
    bool shouldUsePersistentCache { false };
    bool isVideo { false };
    bool renderingCanBeAccelerated { false };
    bool prefersSandboxedParsing { false };
    bool shouldDisableHDR { false };
};

} // namespace WebKit

#endif
