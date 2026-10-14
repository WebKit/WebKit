/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "DOMMatrix2DInit.h"
#include "SVGTransform.h"
#include "SVGTransformValue.h"
#include "SVGValuePropertyList.h"

namespace WebCore {

class SVGTransformList final : public SVGValuePropertyList<SVGTransform> {
    friend class SVGViewSpec;
    using Base = SVGValuePropertyList<SVGTransform>;
    using Base::Base;

public:
    static Ref<SVGTransformList> create()
    {
        return adoptRef(*new SVGTransformList());
    }

    static Ref<SVGTransformList> create(SVGPropertyOwner* owner, SVGPropertyAccess access)
    {
        return adoptRef(*new SVGTransformList(owner, access));
    }

    static Ref<SVGTransformList> create(const SVGTransformList& other, SVGPropertyAccess access)
    {
        return adoptRef(*new SVGTransformList(other, access));
    }

    ExceptionOr<Ref<SVGTransform>> createSVGTransformFromMatrix(DOMMatrix2DInit&& matrixInit)
    {
        Ref svgTransform =  SVGTransform::create();
        svgTransform->setMatrix(WTF::move(matrixInit));
        return svgTransform;
    }

    ExceptionOr<RefPtr<SVGTransform>> consolidate();
    std::optional<AffineTransform> LEGACY_NODELETE concatenate() const;

    bool parse(StringView);
    String valueAsString() const override;

    static std::optional<SVGTransformValue::SVGTransformType> parseTransformType(StringView);
    static std::optional<SVGTransformValue::SVGTransformType> parseTransformType(StringParsingBuffer<Latin1Character>&);
    static std::optional<SVGTransformValue::SVGTransformType> parseTransformType(StringParsingBuffer<char16_t>&);

private:
    // A transform function as read from the attribute, before it is written
    // into an SVGTransform. Holds no SVGMatrix, so reading costs no memory.
    struct ParsedTransform {
        SVGTransformValue::SVGTransformType type;
        std::array<float, 6> values;
        int valueCount;
    };
    using ParsedTransforms = Vector<ParsedTransform, 4>;

    template<typename CharacterType>
    static std::optional<ParsedTransform> parseTransform(SVGTransformValue::SVGTransformType, StringParsingBuffer<CharacterType>&); // Defined in SVGTransformListInlines.h
    static void setTransformValue(SVGTransformValue&, const ParsedTransform&);

    template<typename CharacterType> static bool parseGeneric(StringParsingBuffer<CharacterType>&, ParsedTransforms&);
    template<typename CharacterType> bool parseAndAppend(StringParsingBuffer<CharacterType>&);
    bool parse(StringParsingBuffer<Latin1Character>&);
    bool parse(StringParsingBuffer<char16_t>&);
};

} // namespace WebCore
