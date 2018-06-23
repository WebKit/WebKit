/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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

protected:
    void didFinishInsertingNode() override;

private:
    SVGTextPathElement(const QualifiedName&, Document&);

    virtual ~SVGTextPathElement();

    void clearResourceReferences();

    void buildPendingResource() override;
    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;

    static bool isSupportedAttribute(const QualifiedName&);
    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    bool childShouldCreateRenderer(const Node&) const override;
    bool rendererIsNeeded(const RenderStyle&) override;

    bool selfHasRelativeLengths() const override;
 
    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGTextPathElement)
        DECLARE_ANIMATED_LENGTH(StartOffset, startOffset)
        DECLARE_ANIMATED_ENUMERATION(Method, method, SVGTextPathMethodType)
        DECLARE_ANIMATED_ENUMERATION(Spacing, spacing, SVGTextPathSpacingType)
        DECLARE_ANIMATED_STRING_OVERRIDE(Href, href)
    END_DECLARE_ANIMATED_PROPERTIES
};

} // namespace WebCore
