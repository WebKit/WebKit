/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/FloatRect.h>
#include <WebCore/FloatSize.h>
#include <WebCore/RenderStyleConstants.h>
#include <array>
#include <utility>
#include <wtf/EnumeratedArray.h>

namespace WebCore {

enum class ImagePiece : uint8_t {
    TopLeft,
    Left,
    BottomLeft,
    TopRight,
    Right,
    BottomRight,
    Top,
    Bottom,
    Middle,
};

static constexpr size_t imagePieceCount = std::to_underlying(ImagePiece::Middle) + 1;

static constexpr std::array<ImagePiece, imagePieceCount> allImagePieces {
    ImagePiece::TopLeft,
    ImagePiece::Left,
    ImagePiece::BottomLeft,
    ImagePiece::TopRight,
    ImagePiece::Right,
    ImagePiece::BottomRight,
    ImagePiece::Top,
    ImagePiece::Bottom,
    ImagePiece::Middle,
};

using NinePieceRects = EnumeratedArray<ImagePiece, FloatRect, ImagePiece::Middle>;
using NinePieceScales = EnumeratedArray<ImagePiece, FloatSize, ImagePiece::Middle>;

constexpr bool isCornerPiece(ImagePiece piece)
{
    return piece == ImagePiece::TopLeft || piece == ImagePiece::TopRight || piece == ImagePiece::BottomLeft || piece == ImagePiece::BottomRight;
}

constexpr bool isHorizontalPiece(ImagePiece piece)
{
    return piece == ImagePiece::Top || piece == ImagePiece::Bottom || piece == ImagePiece::Middle;
}

constexpr bool isVerticalPiece(ImagePiece piece)
{
    return piece == ImagePiece::Left || piece == ImagePiece::Right || piece == ImagePiece::Middle;
}

struct NinePieceGeometry {
    NinePieceRects destinationRects;
    NinePieceRects sourceRects;
    NinePieceScales tileScales;
    NinePieceImageRule horizontalRule { NinePieceImageRule::Stretch };
    NinePieceImageRule verticalRule { NinePieceImageRule::Stretch };
    bool fill { false };

    bool isEmptyPiece(ImagePiece piece) const
    {
        return destinationRects[piece].isEmpty() || sourceRects[piece].isEmpty();
    }

    bool shouldSkipPiece(ImagePiece piece) const
    {
        return (piece == ImagePiece::Middle && !fill) || isEmptyPiece(piece);
    }
};

} // namespace WebCore
