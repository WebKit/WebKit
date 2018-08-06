/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedEnumeration.h"
#include "SVGAnimatedLength.h"
#include "SVGExternalResourcesRequired.h"
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
        if (value == "spacingAndGlyphs")
            return SVGLengthAdjustSpacingAndGlyphs;
        if (value == "spacing")
            return SVGLengthAdjustSpacing;
        return SVGLengthAdjustUnknown;
    }
};

class SVGTextContentElement : public SVGGraphicsElement, public SVGExternalResourcesRequired {
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
    const SVGLengthValue& specifiedTextLength() { return m_specifiedTextLength; }

    using AttributeOwnerProxy = SVGAttributeOwnerProxyImpl<SVGTextContentElement, SVGGraphicsElement, SVGExternalResourcesRequired>;
    static AttributeOwnerProxy::AttributeRegistry& attributeRegistry() { return AttributeOwnerProxy::attributeRegistry(); }

    const SVGLengthValue& textLength() const { return m_textLength.currentValue(attributeOwnerProxy()); }
    SVGLengthAdjustType lengthAdjust() const { return m_lengthAdjust.currentValue(attributeOwnerProxy()); }

    RefPtr<SVGAnimatedLength> textLengthAnimated() { return m_textLength.animatedProperty(attributeOwnerProxy()); }
    RefPtr<SVGAnimatedEnumeration> lengthAdjustAnimated() { return m_lengthAdjust.animatedProperty(attributeOwnerProxy()); }

protected:
    SVGTextContentElement(const QualifiedName&, Document&);

    bool isValid() const override { return SVGTests::isValid(); }

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStyleProperties&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    bool selfHasRelativeLengths() const override;

private:
    bool isTextContent() const final { return true; }
    const SVGAttributeOwnerProxy& attributeOwnerProxy() const override { return m_attributeOwnerProxy; }

    static void registerAttributes();
    static bool isKnownAttribute(const QualifiedName& attributeName) { return AttributeOwnerProxy::isKnownAttribute(attributeName); }

    class SVGAnimatedCustomLengthAttribute : public SVGAnimatedLengthAttribute {
    public:
        using SVGAnimatedLengthAttribute::operator=;

        SVGAnimatedCustomLengthAttribute(SVGTextContentElement& element, SVGLengthMode lengthMode)
            : SVGAnimatedLengthAttribute(lengthMode)
            , m_element(element)
        {
        }

        void synchronize(Element&, const QualifiedName& attributeName)
        {
            if (!shouldSynchronize())
                return;
            String string(SVGPropertyTraits<SVGLengthValue>::toString(m_element.m_specifiedTextLength));
            static_cast<Element&>(m_element).setSynchronizedLazyAttribute(attributeName, string);
        }

        RefPtr<SVGAnimatedLength> animatedProperty(const SVGAttributeOwnerProxy& attributeOwnerProxy)
        {
            static NeverDestroyed<SVGLengthValue> defaultTextLength(LengthModeOther);
            if (m_element.m_specifiedTextLength == defaultTextLength)
                m_element.m_textLength.value().newValueSpecifiedUnits(LengthTypeNumber, m_element.getComputedTextLength());

            setShouldSynchronize(true);
            return static_reference_cast<SVGAnimatedLength>(attributeOwnerProxy.lookupOrCreateAnimatedProperty(*this).releaseNonNull());
        }

    private:
        SVGTextContentElement& m_element;
    };

    using SVGAnimatedCustomLengthAttributeAccessor = SVGAnimatedAttributeAccessor<SVGTextContentElement, SVGAnimatedCustomLengthAttribute, AnimatedLength>;

    AttributeOwnerProxy m_attributeOwnerProxy { *this };
    SVGAnimatedCustomLengthAttribute m_textLength { *this, LengthModeOther };
    SVGAnimatedEnumerationAttribute<SVGLengthAdjustType> m_lengthAdjust { SVGLengthAdjustSpacing };
    SVGLengthValue m_specifiedTextLength { LengthModeOther };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::SVGTextContentElement)
    static bool isType(const WebCore::SVGElement& element) { return element.isTextContent(); }
    static bool isType(const WebCore::Node& node) { return is<WebCore::SVGElement>(node) && isType(downcast<WebCore::SVGElement>(node)); }
SPECIALIZE_TYPE_TRAITS_END()
