/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if USE(SYSTEM_PREVIEW)

#include "NativeImage.h"
#include "SystemImage.h"
#include <optional>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

OBJC_CLASS CIContext;

namespace WebCore {

class WEBCORE_EXPORT ARKitBadgeSystemImage final : public SystemImage {
public:
    static Ref<ARKitBadgeSystemImage> create(Image& image)
    {
        return adoptRef(*new ARKitBadgeSystemImage(image));
    }

    static Ref<ARKitBadgeSystemImage> create(RenderingResourceIdentifier renderingResourceIdentifier, FloatSize size)
    {
        return adoptRef(*new ARKitBadgeSystemImage(renderingResourceIdentifier, size));
    }

    virtual ~ARKitBadgeSystemImage() = default;

    void draw(GraphicsContext&, const FloatRect&) const final;

    Image* image() const { return m_image.get(); }
    void setImage(Image& image) { m_image = &image; }

    RenderingResourceIdentifier imageIdentifier() const { return m_renderingResourceIdentifier; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<Ref<ARKitBadgeSystemImage>> decode(Decoder&);

private:
    ARKitBadgeSystemImage(Image& image)
        : SystemImage(SystemImageType::ARKitBadge)
        , m_image(&image)
        , m_imageSize(image.size())
    {
    }

    ARKitBadgeSystemImage(RenderingResourceIdentifier renderingResourceIdentifier, FloatSize size)
        : SystemImage(SystemImageType::ARKitBadge)
        , m_renderingResourceIdentifier(renderingResourceIdentifier)
        , m_imageSize(size)
    {
    }

    RefPtr<Image> m_image;
    RenderingResourceIdentifier m_renderingResourceIdentifier;
    FloatSize m_imageSize;
};

template<class Encoder>
void ARKitBadgeSystemImage::encode(Encoder& encoder) const
{
    ASSERT(m_image);
    ASSERT(m_image->nativeImage());
    encoder << m_image->nativeImage()->renderingResourceIdentifier();
    encoder << m_imageSize;
}

template<class Decoder>
std::optional<Ref<ARKitBadgeSystemImage>> ARKitBadgeSystemImage::decode(Decoder& decoder)
{
    std::optional<RenderingResourceIdentifier> renderingResourceIdentifier;
    decoder >> renderingResourceIdentifier;
    if (!renderingResourceIdentifier)
        return std::nullopt;

    std::optional<FloatSize> imageSize;
    decoder >> imageSize;
    if (!imageSize)
        return std::nullopt;

    return ARKitBadgeSystemImage::create(WTFMove(*renderingResourceIdentifier), WTFMove(*imageSize));
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ARKitBadgeSystemImage)
    static bool isType(const WebCore::SystemImage& systemImage) { return systemImage.systemImageType() == WebCore::SystemImageType::ARKitBadge; }
SPECIALIZE_TYPE_TRAITS_END()

#endif // USE(SYSTEM_PREVIEW)
