/*
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
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

#include "ImageSource.h"

namespace WebCore {

class NativeImageSource : public ImageSource {
public:
    static Ref<NativeImageSource> create(Ref<NativeImage>&&);

private:
    NativeImageSource(Ref<NativeImage>&&);

    IntSize size(ImageOrientation = ImageOrientation::Orientation::FromImage) const final { return m_frame.size(); }
    DestinationColorSpace colorSpace() const final { return m_frame.nativeImage()->colorSpace(); }
    std::optional<Color> singlePixelSolidColor() const final { return m_frame.nativeImage()->singlePixelSolidColor(); }

    const ImageFrame& primaryImageFrame(const std::optional<SubsamplingLevel>& = std::nullopt) final { return m_frame; }

    RefPtr<NativeImage> primaryNativeImage() final { return m_frame.nativeImage(); }

    void dump(TextStream&) const final;

    ImageFrame m_frame;
};

} // namespace WebCore
