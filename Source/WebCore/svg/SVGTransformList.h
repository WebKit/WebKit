/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#include "SVGTransform.h"
#include "SVGTransformable.h"
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

    ExceptionOr<Ref<SVGTransform>> createSVGTransformFromMatrix(const Ref<SVGMatrix>& matrix)
    {
        return SVGTransform::create(matrix->value());
    }

    ExceptionOr<RefPtr<SVGTransform>> consolidate()
    {
        auto result = canAlterList();
        if (result.hasException())
            return result.releaseException();
        ASSERT(result.releaseReturnValue());

        // Spec: If the list was empty, then a value of null is returned.
        if (m_items.isEmpty())
            return nullptr;

        if (m_items.size() == 1)
            return makeRefPtr(at(0).get());

        auto newItem = SVGTransform::create(concatenate());
        clearItems();

        auto item = append(WTFMove(newItem));
        commitChange();
        return makeRefPtr(item.get());
    }

    void parse(const String& value)
    {
        clearItems();

        auto upconvertedCharacters = StringView(value).upconvertedCharacters();
        const UChar* start = upconvertedCharacters;
        if (!parse(start, start + value.length()))
            clearItems();
    }

    AffineTransform concatenate() const
    {
        AffineTransform result;
        for (const auto& transform : m_items)
            result *= transform->matrix()->value();
        return result;
    }

    String valueAsString() const override
    {
        StringBuilder builder;
        for (const auto& transfrom : m_items) {
            if (builder.length())
                builder.append(' ');

            builder.append(transfrom->value().valueAsString());
        }
        return builder.toString();
    }

private:
    bool parse(const UChar*& start, const UChar* end)
    {
        bool delimParsed = false;
        while (start < end) {
            delimParsed = false;
            SVGTransformValue::SVGTransformType type = SVGTransformValue::SVG_TRANSFORM_UNKNOWN;
            skipOptionalSVGSpaces(start, end);

            if (!SVGTransformable::parseAndSkipType(start, end, type))
                return false;

            Ref<SVGTransform> transform = SVGTransform::create(type);
            if (!SVGTransformable::parseTransformValue(type, start, end, transform->value()))
                return false;

            append(WTFMove(transform));
            skipOptionalSVGSpaces(start, end);
            if (start < end && *start == ',') {
                delimParsed = true;
                ++start;
            }

            skipOptionalSVGSpaces(start, end);
        }
        return !delimParsed;
    }
};

} // namespace WebCore
