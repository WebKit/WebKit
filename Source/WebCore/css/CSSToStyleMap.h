/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2012-2014 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include <wtf/Forward.h>

namespace WebCore {

class Animation;
class CSSBorderImageSliceValue;
class CSSBorderImageWidthValue;
class CSSValue;
class FillLayer;
class LengthBox;
class NinePieceImage;
class Quad;
class RenderStyle;
class StyleImage;

enum CSSPropertyID : uint16_t;

struct Length;

namespace Style {
class BuilderState;
}

class CSSToStyleMap {
public:
    explicit CSSToStyleMap(const Style::BuilderState&);

    static void mapFillAttachment(CSSPropertyID, FillLayer&, const CSSValue&);
    static void mapFillClip(CSSPropertyID, FillLayer&, const CSSValue&);
    static void mapFillComposite(CSSPropertyID, FillLayer&, const CSSValue&);
    static void mapFillBlendMode(CSSPropertyID, FillLayer&, const CSSValue&);
    static void mapFillOrigin(CSSPropertyID, FillLayer&, const CSSValue&);
    void mapFillImage(CSSPropertyID, FillLayer&, const CSSValue&) const;
    static void mapFillRepeat(CSSPropertyID, FillLayer&, const CSSValue&);
    void mapFillSize(CSSPropertyID, FillLayer&, const CSSValue&) const;
    void mapFillXPosition(CSSPropertyID, FillLayer&, const CSSValue&) const;
    void mapFillYPosition(CSSPropertyID, FillLayer&, const CSSValue&) const;
    static void mapFillMaskMode(CSSPropertyID, FillLayer&, const CSSValue&);

    void mapAnimationDelay(Animation&, const CSSValue&) const;
    static void mapAnimationDirection(Animation&, const CSSValue&);
    void mapAnimationDuration(Animation&, const CSSValue&) const;
    static void mapAnimationFillMode(Animation&, const CSSValue&);
    void mapAnimationIterationCount(Animation&, const CSSValue&) const;
    void mapAnimationName(Animation&, const CSSValue&) const;
    static void mapAnimationPlayState(Animation&, const CSSValue&);
    static void mapAnimationProperty(Animation&, const CSSValue&);
    void mapAnimationTimeline(Animation&, const CSSValue&) const;
    void mapAnimationTimingFunction(Animation&, const CSSValue&) const;
    static void mapAnimationCompositeOperation(Animation&, const CSSValue&);
    static void mapAnimationAllowsDiscreteTransitions(Animation&, const CSSValue&);
    void mapAnimationRangeStart(Animation&, const CSSValue&) const;
    void mapAnimationRangeEnd(Animation&, const CSSValue&) const;

    void mapNinePieceImage(const CSSValue*, NinePieceImage&) const;
    void mapNinePieceImageSlice(const CSSValue&, NinePieceImage&) const;
    void mapNinePieceImageSlice(const CSSBorderImageSliceValue&, NinePieceImage&) const;
    void mapNinePieceImageWidth(const CSSValue&, NinePieceImage&) const;
    void mapNinePieceImageWidth(const CSSBorderImageWidthValue&, NinePieceImage&) const;
    LengthBox mapNinePieceImageQuad(const CSSValue&) const;
    static void mapNinePieceImageRepeat(const CSSValue&, NinePieceImage&);

private:
    RefPtr<StyleImage> styleImage(const CSSValue&) const;
    LengthBox mapNinePieceImageQuad(const Quad&) const;
    Length mapNinePieceImageSide(const CSSValue&) const;

    const Style::BuilderState& m_builderState;
};

} // namespace WebCore
