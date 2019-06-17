/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "SVGNames.h"
#include "SVGTextContentElement.h"
#include "SVGURIReference.h"

namespace WebCore {

enum SVGTextPathMethodType {
    SVGTextPathMethodUnknown = 0,
    SVGTextPathMethodAlign,
    SVGTextPathMethodStretch
};

enum SVGTextPathSpacingType {
    SVGTextPathSpacingUnknown = 0,
    SVGTextPathSpacingAuto,
    SVGTextPathSpacingExact
};

template<>
struct SVGPropertyTraits<SVGTextPathMethodType> {
    static unsigned highestEnumValue() { return SVGTextPathMethodStretch; }

    static String toString(SVGTextPathMethodType type)
    {
        switch (type) {
        case SVGTextPathMethodUnknown:
            return emptyString();
        case SVGTextPathMethodAlign:
            return "align"_s;
        case SVGTextPathMethodStretch:
            return "stretch"_s;
        }
    
        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static SVGTextPathMethodType fromString(const String& value)
    {
        if (value == "align")
            return SVGTextPathMethodAlign;
        if (value == "stretch")
            return SVGTextPathMethodStretch;
        return SVGTextPathMethodUnknown;
    }
};

template<>
struct SVGPropertyTraits<SVGTextPathSpacingType> {
    static unsigned highestEnumValue() { return SVGTextPathSpacingExact; }

    static String toString(SVGTextPathSpacingType type)
    {
        switch (type) {
        case SVGTextPathSpacingUnknown:
            return emptyString();
        case SVGTextPathSpacingAuto:
            return "auto"_s;
        case SVGTextPathSpacingExact:
            return "exact"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static SVGTextPathSpacingType fromString(const String& value)
    {
        if (value == "auto")
            return SVGTextPathSpacingAuto;
        if (value == "exact")
            return SVGTextPathSpacingExact;
        return SVGTextPathSpacingUnknown;
    }
};

class SVGTextPathElement final : public SVGTextContentElement, public SVGURIReference {
    WTF_MAKE_ISO_ALLOCATED(SVGTextPathElement);
public:
    // Forward declare enumerations in the W3C naming scheme, for IDL generation.
    enum {
        TEXTPATH_METHODTYPE_UNKNOWN = SVGTextPathMethodUnknown,
        TEXTPATH_METHODTYPE_ALIGN = SVGTextPathMethodAlign,
        TEXTPATH_METHODTYPE_STRETCH = SVGTextPathMethodStretch,
        TEXTPATH_SPACINGTYPE_UNKNOWN = SVGTextPathSpacingUnknown,
        TEXTPATH_SPACINGTYPE_AUTO = SVGTextPathSpacingAuto,
        TEXTPATH_SPACINGTYPE_EXACT = SVGTextPathSpacingExact
    };

    static Ref<SVGTextPathElement> create(const QualifiedName&, Document&);

    const SVGLengthValue& startOffset() const { return m_startOffset->currentValue(); }
    SVGTextPathMethodType method() const { return m_method->currentValue<SVGTextPathMethodType>(); }
    SVGTextPathSpacingType spacing() const { return m_spacing->currentValue<SVGTextPathSpacingType>(); }

    SVGAnimatedLength& startOffsetAnimated() { return m_startOffset; }
    SVGAnimatedEnumeration& methodAnimated() { return m_method; }
    SVGAnimatedEnumeration& spacingAnimated() { return m_spacing; }

protected:
    void didFinishInsertingNode() override;

private:
    SVGTextPathElement(const QualifiedName&, Document&);
    virtual ~SVGTextPathElement();

    using PropertyRegistry = SVGPropertyOwnerRegistry<SVGTextPathElement, SVGTextContentElement, SVGURIReference>;
    const SVGPropertyRegistry& propertyRegistry() const final { return m_propertyRegistry; }

    void parseAttribute(const QualifiedName&, const AtomString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    bool childShouldCreateRenderer(const Node&) const override;
    bool rendererIsNeeded(const RenderStyle&) override;

    void clearResourceReferences();
    void buildPendingResource() override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;

    bool selfHasRelativeLengths() const override;

    PropertyRegistry m_propertyRegistry { *this };
    Ref<SVGAnimatedLength> m_startOffset { SVGAnimatedLength::create(this, LengthModeOther) };
    Ref<SVGAnimatedEnumeration> m_method { SVGAnimatedEnumeration::create(this, SVGTextPathMethodAlign) };
    Ref<SVGAnimatedEnumeration> m_spacing { SVGAnimatedEnumeration::create(this, SVGTextPathSpacingExact) };
};

} // namespace WebCore
