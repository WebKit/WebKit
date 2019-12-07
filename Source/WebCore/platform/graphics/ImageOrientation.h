/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#include "AffineTransform.h"
#include "FloatSize.h"
#include <wtf/EnumTraits.h>

namespace WebCore {

struct ImageOrientation {
    enum Orientation {
        FromImage         = 0, // Orientation from the image should be respected.

        // This range intentionally matches the orientation values from the EXIF spec.
        // See JEITA CP-3451, page 18. http://www.exif.org/Exif2-2.PDF
        OriginTopLeft     = 1, // default
        OriginTopRight    = 2, // mirror along y-axis
        OriginBottomRight = 3, // 180 degree rotation
        OriginBottomLeft  = 4, // mirror along the x-axis
        OriginLeftTop     = 5, // mirror along x-axis + 270 degree CW rotation
        OriginRightTop    = 6, // 90 degree CW rotation
        OriginRightBottom = 7, // mirror along x-axis + 90 degree CW rotation
        OriginLeftBottom  = 8, // 270 degree CW rotation

        None              = OriginTopLeft
    };

    ImageOrientation() = default;

    ImageOrientation(Orientation orientation)
        : m_orientation(orientation)
    {
    }

    static Orientation fromEXIFValue(int exifValue)
    {
        return isValidEXIFOrientation(exifValue) ? static_cast<Orientation>(exifValue) : None;
    }

    operator Orientation() const { return m_orientation; }

    bool usesWidthAsHeight() const
    {
        ASSERT(isValidEXIFOrientation(m_orientation));
        // Values 5 through 8 all flip the width/height.
        return m_orientation >= OriginLeftTop && m_orientation <= OriginLeftBottom;
    }

    AffineTransform transformFromDefault(const FloatSize& drawnSize) const
    {
        float w = drawnSize.width();
        float h = drawnSize.height();

        switch (m_orientation) {
        case FromImage:
            ASSERT_NOT_REACHED();
            return AffineTransform();
        case OriginTopLeft:
            return AffineTransform();
        case OriginTopRight:
            return AffineTransform(-1,  0,  0,  1,  w, 0);
        case OriginBottomRight:
            return AffineTransform(-1,  0,  0, -1,  w, h);
        case OriginBottomLeft:
            return AffineTransform( 1,  0,  0, -1,  0, h);
        case OriginLeftTop:
            return AffineTransform( 0,  1,  1,  0,  0, 0);
        case OriginRightTop:
            return AffineTransform( 0,  1, -1,  0,  w, 0);
        case OriginRightBottom:
            return AffineTransform( 0, -1, -1,  0,  w, h);
        case OriginLeftBottom:
            return AffineTransform( 0, -1,  1,  0,  0, h);
        }

        ASSERT_NOT_REACHED();
        return AffineTransform();
    }

private:
    static const Orientation EXIFFirst = OriginTopLeft;
    static const Orientation EXIFLast = OriginLeftBottom;
    static const Orientation First = FromImage;
    static const Orientation Last = EXIFLast;

    static bool isValidOrientation(int orientation)
    {
        return orientation >= static_cast<int>(First) && orientation <= static_cast<int>(Last);
    }

    static bool isValidEXIFOrientation(int orientation)
    {
        return orientation >= static_cast<int>(EXIFFirst) && orientation <= static_cast<int>(EXIFLast);
    }

    Orientation m_orientation { None };
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::ImageOrientation::Orientation> {
    using values = EnumValues<
    WebCore::ImageOrientation::Orientation,
    WebCore::ImageOrientation::Orientation::FromImage,
    WebCore::ImageOrientation::Orientation::OriginTopLeft,
    WebCore::ImageOrientation::Orientation::OriginTopRight,
    WebCore::ImageOrientation::Orientation::OriginBottomRight,
    WebCore::ImageOrientation::Orientation::OriginBottomLeft,
    WebCore::ImageOrientation::Orientation::OriginLeftTop,
    WebCore::ImageOrientation::Orientation::OriginRightTop,
    WebCore::ImageOrientation::Orientation::OriginRightBottom,
    WebCore::ImageOrientation::Orientation::OriginLeftBottom
    >;
};

} // namespace WTF
