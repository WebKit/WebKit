/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ExceptionOr.h"
#include "ImageDataSettings.h"
#include "IntSize.h"
#include "PixelBuffer.h"
#include "PredefinedColorSpace.h"
#include <JavaScriptCore/Uint8ClampedArray.h>
#include <wtf/Forward.h>

namespace WebCore {

class ImageData : public RefCounted<ImageData> {
public:
    WEBCORE_EXPORT static Ref<ImageData> create(PixelBuffer&&);
    WEBCORE_EXPORT static RefPtr<ImageData> create(std::optional<PixelBuffer>&&);
    WEBCORE_EXPORT static RefPtr<ImageData> create(const IntSize&);
    WEBCORE_EXPORT static RefPtr<ImageData> create(const IntSize&, Ref<Uint8ClampedArray>&&, PredefinedColorSpace);
    WEBCORE_EXPORT static ExceptionOr<Ref<ImageData>> createUninitialized(unsigned rows, unsigned pixelsPerRow, PredefinedColorSpace defaultColorSpace, std::optional<ImageDataSettings> = std::nullopt);
    WEBCORE_EXPORT static ExceptionOr<Ref<ImageData>> create(unsigned sw, unsigned sh, std::optional<ImageDataSettings>);
    WEBCORE_EXPORT static ExceptionOr<Ref<ImageData>> create(Ref<Uint8ClampedArray>&&, unsigned sw, std::optional<unsigned> sh, std::optional<ImageDataSettings>);

    WEBCORE_EXPORT ~ImageData();

    static PredefinedColorSpace computeColorSpace(std::optional<ImageDataSettings>, PredefinedColorSpace defaultColorSpace = PredefinedColorSpace::SRGB);

    const IntSize& size() const { return m_size; }

    int width() const { return m_size.width(); }
    int height() const { return m_size.height(); }
    Uint8ClampedArray& data() const { return m_data.get(); }
    PredefinedColorSpace colorSpace() const { return m_colorSpace; }

    PixelBuffer pixelBuffer() const;

private:
    explicit ImageData(const IntSize&, Ref<JSC::Uint8ClampedArray>&&, PredefinedColorSpace);

    IntSize m_size;
    Ref<JSC::Uint8ClampedArray> m_data;
    PredefinedColorSpace m_colorSpace;
};

WEBCORE_EXPORT TextStream& operator<<(TextStream&, const ImageData&);

} // namespace WebCore
