/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2018-2019 Apple Inc. All rights reserved.
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

#include "SVGGraphicsElement.h"

namespace WebCore {

struct DOMPointInit;

enum SVGLengthAdjustType {
    SVGLengthAdjustUnknown,
    SVGLengthAdjustSpacing,
    SVGLengthAdjustSpacingAndGlyphs
};

template<> struct SVGPropertyTraits<SVGLengthAdjustType> {
    static unsigned highestEnumValue() { return SVGLengthAdjustSpacingAndGlyphs; }

    static String toString(SVGLengthAdjustType type)
    {
        switch (type) {
        case SVGLengthAdjustUnknown:
            return emptyString();
        case SVGLengthAdjustSpacing:
            return "spacing"_s;
        case SVGLengthAdjustSpacingAndGlyphs:
            return "spacingAndGlyphs"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static SVGLengthAdjustType fromString(const String& value)
    {
        if (value == "spacingAndGlyphs"_s)
            return SVGLengthAdjustSpacingAndGlyphs;
        if (value == "spacing"_s)
            return SVGLengthAdjustSpacing;
        return SVGLengthAdjustUnknown;
    }
};

class SVGTextContentElement : public SVGGraphicsElement {
    WTF_MAKE_ISO_ALLOCATED(SVGTextContentElement);
public:
    enum {
        LENGTHADJUST_UNKNOWN = SVGLengthAdjustUnknown,
        LENGTHADJUST_SPACING = SVGLengthAdjustSpacing,
        LENGTHADJUST_SPACINGANDGLYPHS = SVGLengthAdjustSpacingAndGlyphs
    };

    unsigned getNumberOfChars();
    float getComputedTextLength();
    ExceptionOr<float> getSubStringLength(unsigned charnum, unsigned nchars);
    ExceptionOr<Ref<SVGPoint>> getStartPositionOfChar(unsigned charnum);
    ExceptionOr<Ref<SVGPoint>> getEndPositionOfChar(unsigned charnum);
    ExceptionOr<Ref<SVGRect>> getExtentOfChar(unsigned charnum);
    ExceptionOr<float> getRotationOfChar(unsigned charnum);
    int getCharNumAtPosition(DOMPointInit&&);
    ExceptionOr<void> selectSubString(unsigned charnum, unsigned nchars);

    static SVGTextContentElement* elementFromRenderer(RenderObject*);

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGTextContentElement, SVGGraphicsElement>;

    const SVGLengthValue& specifiedTextLength() const { return m_specifiedTextLength; }
    const SVGLengthValue& textLength() const { return m_textLength->currentValue(); }
    SVGLengthAdjustType lengthAdjust() const { return m_lengthAdjust->currentValue<SVGLengthAdjustType>(); }

    SVGAnimatedLength& textLengthAnimated();
    SVGAnimatedEnumeration& lengthAdjustAnimated() { return m_lengthAdjust; }

protected:
    SVGTextContentElement(const QualifiedName&, Document&, UniqueRef<SVGPropertyRegistry>&&);

    bool isValid() const override { return SVGTests::isValid(); }

    void attributeChanged(const QualifiedName&, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason) override;
    bool hasPresentationalHintsForAttribute(const QualifiedName&) const override;
    void collectPresentationalHintsForAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool selfHasRelativeLengths() const override;

private:
    bool isTextContent() const final { return true; }

    Ref<SVGAnimatedLength> m_textLength { SVGAnimatedLength::create(this, SVGLengthMode::Other) };
    Ref<SVGAnimatedEnumeration> m_lengthAdjust { SVGAnimatedEnumeration::create(this, SVGLengthAdjustSpacing) };
    SVGLengthValue m_specifiedTextLength { SVGLengthMode::Other };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SVGTextContentElement)
    static bool isType(const WebCore::SVGElement& element) { return element.isTextContent(); }
    static bool isType(const WebCore::Node& node)
    {
        auto* svgElement = dynamicDowncast<WebCore::SVGElement>(node);
        return svgElement && isType(*svgElement);
    }
SPECIALIZE_TYPE_TRAITS_END()
