/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGStyledElement_h
#define SVGStyledElement_h

#if ENABLE(SVG)
#include "SVGAnimatedString.h"
#include "SVGLocatable.h"
#include "SVGStylable.h"
#include <wtf/HashSet.h>

namespace WebCore {

void mapAttributeToCSSProperty(HashMap<AtomicStringImpl*, int>* propertyNameToIdMap, const QualifiedName& attrName);

class SVGStyledElement : public SVGElement,
                         public SVGStylable {
public:
    virtual ~SVGStyledElement();

    virtual String title() const;

    bool hasRelativeLengths() const { return !m_elementsWithRelativeLengths.isEmpty(); }

    virtual bool supportsMarkers() const { return false; }

    virtual PassRefPtr<CSSValue> getPresentationAttribute(const String& name);

    bool isKnownAttribute(const QualifiedName&);

    bool instanceUpdatesBlocked() const;
    void setInstanceUpdatesBlocked(bool);

    bool hasPendingResources() const;
    void setHasPendingResources();
    void clearHasPendingResourcesIfPossible();

    virtual void animatedPropertyTypeForAttribute(const QualifiedName&, Vector<AnimatedPropertyType>&);
    static bool isAnimatableCSSProperty(const QualifiedName&);

    virtual AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope) const;

    virtual CSSStyleDeclaration* style() { return StyledElement::style(); }
    virtual bool needsPendingResourceHandling() const { return true; }

protected: 
    SVGStyledElement(const QualifiedName&, Document*, ConstructionType = CreateSVGElement);
    virtual bool rendererIsNeeded(const NodeRenderingContext&);

    virtual void parseMappedAttribute(Attribute*);
    virtual void svgAttributeChanged(const QualifiedName&);

    virtual void attach();
    virtual void insertedIntoDocument();
    virtual void removedFromDocument();
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    static int cssPropertyIdForSVGAttributeName(const QualifiedName&);
    void updateRelativeLengthsInformation() { updateRelativeLengthsInformation(selfHasRelativeLengths(), this); }
    void updateRelativeLengthsInformation(bool hasRelativeLengths, SVGStyledElement*);

    virtual bool selfHasRelativeLengths() const { return false; }
    virtual void buildPendingResource() { }

private:
    virtual bool isStyled() const { return true; }

    virtual bool isKeyboardFocusable(KeyboardEvent*) const;
    virtual bool isMouseFocusable() const;

    void buildPendingResourcesIfNeeded();

    HashSet<SVGStyledElement*> m_elementsWithRelativeLengths;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGStyledElement)
        DECLARE_ANIMATED_STRING(ClassName, className)
    END_DECLARE_ANIMATED_PROPERTIES
};

inline SVGStyledElement* toSVGStyledElement(Node* node)
{
    ASSERT(!node || (node->isStyledElement() && node->isSVGElement()));
    return static_cast<SVGStyledElement*>(node);
}

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // SVGStyledElement
