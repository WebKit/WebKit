/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "ImageBuffer.h"
#include <wtf/OptionSet.h>

namespace WebCore {

enum class SerializationState : uint8_t {
    OriginClean              = 1 << 0,
    PremultiplyAlpha         = 1 << 1,
    ForciblyPremultiplyAlpha = 1 << 2
};

class ImageBitmapBacking {
public:
    ImageBitmapBacking(std::unique_ptr<ImageBuffer>&&, OptionSet<SerializationState> = SerializationState::OriginClean);

    ImageBuffer* buffer() const;
    std::unique_ptr<ImageBuffer> takeImageBuffer();

    unsigned width() const;
    unsigned height() const;

    bool originClean() const { return m_serializationState.contains(SerializationState::OriginClean); }

    bool premultiplyAlpha() const { return m_serializationState.contains(SerializationState::PremultiplyAlpha); }
    // When WebGL consumes an Image coming from an ImageBitmap's ImageBuffer, it typically honors
    // the alpha mode of that native image - CGImageAlphaInfo in the Core Graphics backend. For
    // ImageBitmaps created from ImageBitmaps, this information is not accurate, and callers must be
    // told to ignore the alpha mode, and forcibly premultiply the alpha channel.
    bool forciblyPremultiplyAlpha() const { return m_serializationState.contains(SerializationState::ForciblyPremultiplyAlpha); }

    OptionSet<SerializationState> serializationState() const { return m_serializationState; }

private:
    std::unique_ptr<ImageBuffer> m_bitmapData;
    OptionSet<SerializationState> m_serializationState;
};

} // namespace WebCore
