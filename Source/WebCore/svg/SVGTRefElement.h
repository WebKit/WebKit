/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#ifndef SVGTRefElement_h
#define SVGTRefElement_h

#include "SVGTextPositioningElement.h"
#include "SVGURIReference.h"

namespace WebCore {

class SVGTRefTargetEventListener;

class SVGTRefElement final : public SVGTextPositioningElement, public SVGURIReference {
public:
    static Ref<SVGTRefElement> create(const QualifiedName&, Document&);

private:
    friend class SVGTRefTargetEventListener;

    SVGTRefElement(const QualifiedName&, Document&);
    virtual ~SVGTRefElement();

    void parseAttribute(const QualifiedName&, const AtomicString&) override;
    void svgAttributeChanged(const QualifiedName&) override;

    RenderPtr<RenderElement> createElementRenderer(Ref<RenderStyle>&&, const RenderTreePosition&) override;
    bool childShouldCreateRenderer(const Node&) const override;
    bool rendererIsNeeded(const RenderStyle&) override;

    InsertionNotificationRequest insertedInto(ContainerNode&) override;
    void removedFrom(ContainerNode&) override;
    void finishedInsertingSubtree() override;

    void clearTarget() override;

    void updateReferencedText(Element*);

    void detachTarget();

    void buildPendingResource() override;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGTRefElement)
        DECLARE_ANIMATED_STRING_OVERRIDE(Href, href)
    END_DECLARE_ANIMATED_PROPERTIES

    Ref<SVGTRefTargetEventListener> m_targetListener;
};

} // namespace WebCore

#endif
