/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGCursorElement_h
#define SVGCursorElement_h

#include "SVGAnimatedBoolean.h"
#include "SVGAnimatedLength.h"
#include "SVGAnimatedString.h"
#include "SVGElement.h"
#include "SVGExternalResourcesRequired.h"
#include "SVGTests.h"
#include "SVGURIReference.h"

namespace WebCore {

class CSSCursorImageValue;

class SVGCursorElement final : public SVGElement,
                               public SVGTests,
                               public SVGExternalResourcesRequired,
                               public SVGURIReference {
public:
    static Ref<SVGCursorElement> create(const QualifiedName&, Document&);

    virtual ~SVGCursorElement();

    void addClient(CSSCursorImageValue&);
    void removeClient(CSSCursorImageValue&);

private:
    SVGCursorElement(const QualifiedName&, Document&);

    bool isValid() const final { return SVGTests::isValid(); }

    static bool isSupportedAttribute(const QualifiedName&);
    void parseAttribute(const QualifiedName&, const AtomicString&) final;
    void svgAttributeChanged(const QualifiedName&) final;

    bool rendererIsNeeded(const RenderStyle&) final { return false; }

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const final;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGCursorElement)
        DECLARE_ANIMATED_LENGTH(X, x)
        DECLARE_ANIMATED_LENGTH(Y, y)
        DECLARE_ANIMATED_STRING_OVERRIDE(Href, href)
        DECLARE_ANIMATED_BOOLEAN_OVERRIDE(ExternalResourcesRequired, externalResourcesRequired)
    END_DECLARE_ANIMATED_PROPERTIES

    // SVGTests
    void synchronizeRequiredFeatures() final { SVGTests::synchronizeRequiredFeatures(this); }
    void synchronizeRequiredExtensions() final { SVGTests::synchronizeRequiredExtensions(this); }
    void synchronizeSystemLanguage() final { SVGTests::synchronizeSystemLanguage(this); }

    HashSet<CSSCursorImageValue*> m_clients;
};

} // namespace WebCore

#endif
