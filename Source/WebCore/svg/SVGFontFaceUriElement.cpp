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
#include "SVGFontFaceElement.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(SVGFontFaceUriElement);
    
using namespace SVGNames;
    
inline SVGFontFaceUriElement::SVGFontFaceUriElement(const QualifiedName& tagName, Document& document)
    : SVGElement(tagName, document)
{
    ASSERT(hasTagName(font_face_uriTag));
}

Ref<SVGFontFaceUriElement> SVGFontFaceUriElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new SVGFontFaceUriElement(tagName, document));
}

SVGFontFaceUriElement::~SVGFontFaceUriElement()
{
    if (m_cachedFont)
        m_cachedFont->removeClient(*this);
}

Ref<CSSFontFaceSrcValue> SVGFontFaceUriElement::srcValue() const
{
    auto src = CSSFontFaceSrcValue::create(getAttribute(SVGNames::hrefAttr, XLinkNames::hrefAttr), LoadedFromOpaqueSource::No);
    AtomString value(attributeWithoutSynchronization(formatAttr));
    src.get().setFormat(value.isEmpty() ? "svg" : value); // Default format
    return src;
}

void SVGFontFaceUriElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == SVGNames::hrefAttr || name == XLinkNames::hrefAttr)
        loadFont();
    else
        SVGElement::parseAttribute(name, value);
}

void SVGFontFaceUriElement::childrenChanged(const ChildChange& change)
{
    SVGElement::childrenChanged(change);

    if (!parentNode() || !parentNode()->hasTagName(font_face_srcTag))
        return;
    
    auto grandparent = makeRefPtr(parentNode()->parentNode());
    if (grandparent && grandparent->hasTagName(font_faceTag))
        downcast<SVGFontFaceElement>(*grandparent).rebuildFontFace();
}

Node::InsertedIntoAncestorResult SVGFontFaceUriElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    loadFont();
    return SVGElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
}

static bool isSVGFontTarget(const SVGFontFaceUriElement& element)
{
    Ref<CSSFontFaceSrcValue> srcValue(element.srcValue());
    return srcValue->isSVGFontTarget();
}

void SVGFontFaceUriElement::loadFont()
{
    if (m_cachedFont)
        m_cachedFont->removeClient(*this);

    const AtomString& href = getAttribute(SVGNames::hrefAttr, XLinkNames::hrefAttr);
    if (!href.isNull()) {
        ResourceLoaderOptions options = CachedResourceLoader::defaultCachedResourceOptions();
        options.contentSecurityPolicyImposition = isInUserAgentShadowTree() ? ContentSecurityPolicyImposition::SkipPolicyCheck : ContentSecurityPolicyImposition::DoPolicyCheck;

        CachedResourceLoader& cachedResourceLoader = document().cachedResourceLoader();
        CachedResourceRequest request(ResourceRequest(document().completeURL(href)), options);
        request.setInitiator(*this);
        m_cachedFont = cachedResourceLoader.requestFont(WTFMove(request), isSVGFontTarget(*this)).value_or(nullptr);
        if (m_cachedFont) {
            m_cachedFont->addClient(*this);
            m_cachedFont->beginLoadIfNeeded(cachedResourceLoader);
        }
    } else
        m_cachedFont = nullptr;
}

}
