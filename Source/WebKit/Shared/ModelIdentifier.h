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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

namespace WebKit {

#if ENABLE(ARKIT_INLINE_PREVIEW)

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
#import <WebCore/GraphicsLayer.h>
#endif

struct ModelIdentifier {
#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    String uuid;
#elif ENABLE(ARKIT_INLINE_PREVIEW_IOS)
    WebCore::GraphicsLayer::PlatformLayerID layerIdentifier;
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ModelIdentifier> decode(Decoder&);
};

template<class Encoder> void ModelIdentifier::encode(Encoder& encoder) const
{
#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    encoder << uuid;
#elif ENABLE(ARKIT_INLINE_PREVIEW_IOS)
    encoder << layerIdentifier;
#endif
}

template<class Decoder> std::optional<ModelIdentifier> ModelIdentifier::decode(Decoder& decoder)
{
#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    std::optional<String> uuid;
    decoder >> uuid;
    if (!uuid)
        return std::nullopt;
    return { { WTFMove(*uuid) } };
#elif ENABLE(ARKIT_INLINE_PREVIEW_IOS)
    std::optional<WebCore::GraphicsLayer::PlatformLayerID> layerIdentifier;
    decoder >> layerIdentifier;
    if (!layerIdentifier)
        return std::nullopt;
    return { { *layerIdentifier } };
#endif
}

#endif

} // namespace WebKit

