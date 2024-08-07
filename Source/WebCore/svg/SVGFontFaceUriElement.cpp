/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SVGFontFaceUriElement.h"

#include "CSSFontFaceSrcValue.h"
#include "CachedFont.h"
#include "CachedResourceLoader.h"
#include "CachedResourceRequest.h"
#include "Document.h"
#include "SVGElementInlines.h"
#include "SVGElementTypeHelpers.h"
#include "SVGFontFaceElement.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(SVGFontFaceUriElement);
    
using namespace SVGNames;
    
inline SVGFontFaceUriElement::SVGFontFaceUriElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document, makeUniqueRef<PropertyRegistry>(*this))
{
    ASSERT(hasTagName(font_face_uriTag));
}

Ref<SVGFontFaceUriElement> SVGFontFaceUriElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFontFaceUriElement(tagName, document));
}

SVGFontFaceUriElement::~SVGFontFaceUriElement()
{
    if (CachedResourceHandle cachedFont = m_cachedFont)
        cachedFont->removeClient(*this);
}

Ref<CSSFontFaceSrcResourceValue> SVGFontFaceUriElement::createSrcValue() const
{
    ResolvedURL location;
    location.specifiedURLString = getAttribute(SVGNames::hrefAttr, XLinkNames::hrefAttr);
    if (!location.specifiedURLString.isNull())
        location.resolvedURL = document().completeURL(location.specifiedURLString);
    auto& format = attributeWithoutSynchronization(formatAttr);
    return CSSFontFaceSrcResourceValue::create(WTFMove(location), format.isEmpty() ? "svg"_s : format.string(), { FontTechnology::ColorSvg }, LoadedFromOpaqueSource::No);
}

void SVGFontFaceUriElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == SVGNames::hrefAttr || name == XLinkNames::hrefAttr)
        loadFont();

    SVGElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

void SVGFontFaceUriElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (!parentNode() || !parentNode()->hasTagName(font_face_srcTag))
        return;
    
    if (RefPtr grandParent = dynamicDowncast<SVGFontFaceElement>(parentNode()->parentNode()))
        grandParent->rebuildFontFace();
}

Node::InsertedIntoAncestorResult SVGFontFaceUriElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    loadFont();
    return SVGElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
}

static bool isSVGFontTarget(const SVGFontFaceUriElement& element)
{
    auto& format = element.attributeWithoutSynchronization(formatAttr);
    return format.isEmpty() || equalLettersIgnoringASCIICase(format, "svg"_s);
}

void SVGFontFaceUriElement::loadFont()
{
    if (CachedResourceHandle cachedFont = m_cachedFont)
        cachedFont->removeClient(*this);

    const AtomString& href = getAttribute(SVGNames::hrefAttr, XLinkNames::hrefAttr);
    if (!href.isNull()) {
        ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
        options.contentSecurityPolicyImposition = isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;

        Ref cachedResourceLoader = document().cachedResourceLoader();
        CachedResourceRequest request(ResourceRequest(document().completeURL(href)), options);
        request.setInitiator(*this);
        m_cachedFont = cachedResourceLoader->requestFont(WTFMove(request), isSVGFontTarget(*this)).value_or(nullptr);
        if (CachedResourceHandle cachedFont = m_cachedFont) {
            cachedFont->addClient(*this);
            cachedFont->beginLoadIfNeeded(cachedResourceLoader);
        }
    } else
        m_cachedFont = nullptr;
}

}
