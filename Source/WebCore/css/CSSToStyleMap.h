/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "CSSPropertyNames.h"
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class Animation;
class CSSBorderImageSliceValue;
class CSSBorderImageWidthValue;
class CSSPrimitiveValue;
class CSSValue;
class FillLayer;
class LengthBox;
class NinePieceImage;
class Quad;
class RenderStyle;
class StyleImage;

namespace Style {
class BuilderState;
}

class CSSToStyleMap {
    WTF_MAKE_NONCOPYABLE(CSSToStyleMap);
    WTF_MAKE_FAST_ALLOCATED;

public:
    CSSToStyleMap(Style::BuilderState&);

    void mapFillAttachment(CSSPropertyID, FillLayer&, const CSSValue&);
    void mapFillClip(CSSPropertyID, FillLayer&, const CSSValue&);
    void mapFillComposite(CSSPropertyID, FillLayer&, const CSSValue&);
    void mapFillBlendMode(CSSPropertyID, FillLayer&, const CSSValue&);
    void mapFillOrigin(CSSPropertyID, FillLayer&, const CSSValue&);
    void mapFillImage(CSSPropertyID, FillLayer&, CSSValue&);
    void mapFillRepeat(CSSPropertyID, FillLayer&, const CSSValue&);
    void mapFillSize(CSSPropertyID, FillLayer&, const CSSValue&);
    void mapFillXPosition(CSSPropertyID, FillLayer&, const CSSValue&);
    void mapFillYPosition(CSSPropertyID, FillLayer&, const CSSValue&);
    void mapFillMaskMode(CSSPropertyID, FillLayer&, const CSSValue&);

    void mapAnimationDelay(Animation&, const CSSValue&);
    void mapAnimationDirection(Animation&, const CSSValue&);
    void mapAnimationDuration(Animation&, const CSSValue&);
    void mapAnimationFillMode(Animation&, const CSSValue&);
    void mapAnimationIterationCount(Animation&, const CSSValue&);
    void mapAnimationName(Animation&, const CSSValue&);
    void mapAnimationPlayState(Animation&, const CSSValue&);
    void mapAnimationProperty(Animation&, const CSSValue&);
    void mapAnimationTimingFunction(Animation&, const CSSValue&);
    void mapAnimationCompositeOperation(Animation&, const CSSValue&);

    void mapNinePieceImage(CSSValue*, NinePieceImage&);
    void mapNinePieceImageSlice(CSSValue&, NinePieceImage&);
    void mapNinePieceImageSlice(CSSBorderImageSliceValue&, NinePieceImage&);
    void mapNinePieceImageWidth(CSSValue&, NinePieceImage&);
    void mapNinePieceImageWidth(CSSBorderImageWidthValue&, NinePieceImage&);
    LengthBox mapNinePieceImageQuad(CSSValue&);
    void mapNinePieceImageRepeat(CSSValue&, NinePieceImage&);
    void mapNinePieceImageRepeat(CSSPrimitiveValue&, NinePieceImage&);

private:
    RenderStyle* style() const;
    bool useSVGZoomRules() const;
    RefPtr<StyleImage> styleImage(CSSValue&);
    LengthBox mapNinePieceImageQuad(Quad&);

    // FIXME: This type can merge into BuilderState.
    Style::BuilderState& m_builderState;
};

} // namespace WebCore
