/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2007-2008 Torch Mobile, Inc.
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

#include "AffineTransform.h"
#include "Image.h"

#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

#if USE(CG)
typedef struct CGPattern* CGPatternRef;
typedef CGPatternRef PlatformPatternPtr;
#elif USE(DIRECT2D)
interface ID2D1BitmapBrush;
typedef ID2D1BitmapBrush* PlatformPatternPtr;
namespace WebCore {
class PlatformContextDirect2D;
}
typedef WebCore::PlatformContextDirect2D PlatformGraphicsContext;
#elif USE(CAIRO)
typedef struct _cairo_pattern cairo_pattern_t;
typedef cairo_pattern_t* PlatformPatternPtr;
#elif PLATFORM(HAIKU)
#include <interface/GraphicsDefs.h>
typedef pattern* PlatformPatternPtr;
#endif

namespace WebCore {

class AffineTransform;
class GraphicsContext;
class Image;
class ImageHandle;

class Pattern final : public RefCounted<Pattern> {
public:
    WEBCORE_EXPORT static Ref<Pattern> create(Ref<Image>&& tileImage, bool repeatX, bool repeatY);
    WEBCORE_EXPORT ~Pattern();

    Image& tileImage() const { return m_tileImage.get(); }

    // Pattern space is an abstract space that maps to the default user space by the transformation 'userSpaceTransformation'
#if !USE(DIRECT2D)
    PlatformPatternPtr createPlatformPattern(const AffineTransform& userSpaceTransformation) const;
#else
    PlatformPatternPtr createPlatformPattern(const GraphicsContext&, float alpha, const AffineTransform& userSpaceTransformation) const;
#endif
    WEBCORE_EXPORT void setPatternSpaceTransform(const AffineTransform& patternSpaceTransformation);
    const AffineTransform& patternSpaceTransform() const { return m_patternSpaceTransformation; };
    bool repeatX() const { return m_repeatX; }
    bool repeatY() const { return m_repeatY; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<Pattern>> decode(Decoder&);

private:
    Pattern(Ref<Image>&&, bool repeatX, bool repeatY);

    Ref<Image> m_tileImage;
    AffineTransform m_patternSpaceTransformation;
    bool m_repeatX;
    bool m_repeatY;
};

template<class Encoder>
void Pattern::encode(Encoder& encoder) const
{
    ImageHandle imageHandle;
    imageHandle.image = m_tileImage.ptr();
    encoder << imageHandle;
    encoder << m_patternSpaceTransformation;
    encoder << m_repeatX;
    encoder << m_repeatY;
}

template<class Decoder>
Optional<Ref<Pattern>> Pattern::decode(Decoder& decoder)
{
    Optional<ImageHandle> imageHandle;
    decoder >> imageHandle;
    if (!imageHandle)
        return WTF::nullopt;

    Optional<AffineTransform> patternSpaceTransformation;
    decoder >> patternSpaceTransformation;
    if (!patternSpaceTransformation)
        return WTF::nullopt;

    Optional<bool> repeatX;
    decoder >> repeatX;
    if (!repeatX)
        return WTF::nullopt;

    Optional<bool> repeatY;
    decoder >> repeatY;
    if (!repeatY)
        return WTF::nullopt;

    auto pattern = Pattern::create(imageHandle->image.releaseNonNull(), *repeatX, *repeatY);
    pattern->setPatternSpaceTransform(*patternSpaceTransformation);
    return pattern;
}

} //namespace

