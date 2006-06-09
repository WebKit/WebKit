/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include "config.h"
#if SVG_SUPPORT
#include "SVGDOMImplementation.h"

#include "CSSStyleSheet.h"
#include "Document.h"
#include "DocumentType.h"
#include "ExceptionCode.h"
#include "MediaList.h"
#include "PlatformString.h"
#include "SVGDocument.h"
#include "SVGRenderStyle.h"
#include "SVGSVGElement.h"
#include "ksvg.h"
#include <wtf/HashSet.h>

using namespace WebCore;

static const HashSet<String>& svgFeatureSet()
{
    static HashSet<String>* svgFeatures = 0;
    if (!svgFeatures) {
        svgFeatures = new HashSet<String>;
        
        // 1.1 features
        svgFeatures->add("SVG");
        svgFeatures->add("SVGDOM");
        svgFeatures->add("SVG-STATIC");
        svgFeatures->add("SVGDOM-STATIC");
        svgFeatures->add("SVG-ANIMATION");
        svgFeatures->add("SVGDOM-ANIMATION");
        svgFeatures->add("SVG-DYNAMIC");
        svgFeatures->add("COREATTRIBUTE");
        svgFeatures->add("STRUCTURE");
        svgFeatures->add("CONTAINERATTRIBUTE");
        svgFeatures->add("CONDITIOANLPROCESSING");
        svgFeatures->add("IMAGE");
        svgFeatures->add("STYLE");
        svgFeatures->add("VIEWPORTATTRIBUTE");
        svgFeatures->add("SHAPE");
        svgFeatures->add("TEXT");
        svgFeatures->add("PAINTATTRIBUTE");
        svgFeatures->add("OPACITYATTRIBUTE");
        svgFeatures->add("GRAPHICSATTRIBUTE");
        svgFeatures->add("MARKER");
        svgFeatures->add("COLORPROFILE");
        svgFeatures->add("GRADIENT");
        svgFeatures->add("PATTERN");
        svgFeatures->add("CLIP");
        svgFeatures->add("MASK");
        svgFeatures->add("FILTER");
        svgFeatures->add("XLINKATTRIBUTE");
        svgFeatures->add("FONT");
        svgFeatures->add("EXTENSIBILITY");

        // 1.0 features
        //svgFeatures->add("SVG");
        svgFeatures->add("SVG.STATIC");
        svgFeatures->add("SVG.ANIMATION");
        svgFeatures->add("SVG.DYNAMIC");
        svgFeatures->add("DOM");
        svgFeatures->add("DOM.SVG");
        svgFeatures->add("DOM.SVG.STATIC");
        svgFeatures->add("DOM.SVG.ANIMATION");
        svgFeatures->add("DOM.SVG.DYNAMIC");
        svgFeatures->add("SVG.ALL");
        svgFeatures->add("DOM.SVG.ALL");
    }
    return *svgFeatures;
}

SVGDOMImplementation::SVGDOMImplementation() : DOMImplementation()
{
    m_animationContext = false;
}

SVGDOMImplementation::~SVGDOMImplementation()
{
}

SVGDOMImplementation *SVGDOMImplementation::instance()
{
    static RefPtr<SVGDOMImplementation> i = new SVGDOMImplementation;
    return i.get();
}

bool SVGDOMImplementation::hasFeature(StringImpl *featureImpl, StringImpl *versionImpl) const
{
    String feature = (featureImpl ? featureImpl->upper(): String());
    String version(versionImpl);

    if ((version.isEmpty() || version == "1.1") && feature.startsWith("HTTP://WWW.W3.ORG/TR/SVG11/FEATURE#")) {
        if (svgFeatureSet().contains(feature.right(feature.length() - 35)))
            return true;
    }

    if ((version.isEmpty() || version == "1.0") && feature.startsWith("ORG.W3C.")) {
        if (svgFeatureSet().contains(feature.right(feature.length() - 8)))
            return true;
    }

    return DOMImplementation::hasFeature(featureImpl, versionImpl);
}

PassRefPtr<DocumentType> SVGDOMImplementation::createDocumentType(StringImpl *qualifiedNameImpl, StringImpl *publicId, StringImpl *systemId, ExceptionCode& ec) const
{
    String qualifiedName(qualifiedNameImpl);
#if 0
    // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
    if (!qualifiedName.isEmpty() && !Helper::ValidateAttributeName(qualifiedNameImpl)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }

    // NAMESPACE_ERR: Raised if no qualifiedName supplied (not mentioned in the spec!)
    if (qualifiedName.isEmpty()) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    // NAMESPACE_ERR: Raised if the qualifiedName is malformed.
    Helper::CheckMalformedQualifiedName(qualifiedNameImpl);
#endif

    return new DocumentType(0, qualifiedName, publicId, systemId);
}

PassRefPtr<Document> SVGDOMImplementation::createDocument(StringImpl *namespaceURI, StringImpl *qualifiedNameImpl, DocumentType *doctype, ExceptionCode& ec) const
{
    return createDocument(namespaceURI, qualifiedNameImpl, doctype, true, 0, ec);
}

PassRefPtr<Document> SVGDOMImplementation::createDocument(StringImpl *namespaceURIImpl, StringImpl *qualifiedNameImpl, DocumentType *doctype, bool createDocElement, FrameView *view, ExceptionCode& ec) const
{
    String namespaceURI(namespaceURIImpl);
    String qualifiedName(qualifiedNameImpl);
    if ((namespaceURI != SVGNames::svgNamespaceURI) || (qualifiedName != "svg" && qualifiedName != "svg:svg"))
        return DOMImplementation::instance()->createDocument(namespaceURIImpl, qualifiedNameImpl, doctype, ec);

#if 0
    int dummy;
    Helper::CheckQualifiedName(qualifiedNameImpl, namespaceURIImpl, dummy, true /*nameCanBeNull*/, true /*nameCanBeEmpty, see #61650*/);
#endif

    // WRONG_DOCUMENT_ERR: Raised if docType has already been used with a different
    //                     document or was created from a different implementation.
    if (doctype != 0 && doctype->ownerDocument() != 0) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    RefPtr<SVGDocument> doc = new SVGDocument(const_cast<SVGDOMImplementation *>(this), view);

    // TODO : what to do when doctype is null?
    if (doctype)
        doc->setDocType(doctype);

    // Add root element...
    if (createDocElement)
        doc->appendChild(doc->createElementNS(namespaceURI, qualifiedName, ec), ec);

    return doc.release();
}

PassRefPtr<CSSStyleSheet> SVGDOMImplementation::createCSSStyleSheet(StringImpl *title, StringImpl *media) const
{
    // TODO : check whether media is valid
    CSSStyleSheet *parent = 0;
    RefPtr<CSSStyleSheet> sheet = new CSSStyleSheet(parent);
    //sheet->setTitle(title);
    // http://www.w3.org/TR/DOM-Level-2-Style/css.html#CSS-DOMImplementationCSS says
    // that media descriptors should be used, even though svg:style has only
    // css media types. Hence the third parameter (true).
    sheet->setMedia(new MediaList(sheet.get(), media, true)); 
    return sheet.release();
}

DocumentType *SVGDOMImplementation::defaultDocumentType() const
{
    return 0;
    /*
    return createDocumentType("svg:svg", "-//W3C//DTD SVG20010904//EN",
                             "http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd");
    */
}

bool SVGDOMImplementation::inAnimationContext() const
{
    return m_animationContext;
}

void SVGDOMImplementation::setAnimationContext(bool value)
{
    m_animationContext = value;
}

PassRefPtr<Document> SVGDOMImplementation::createDocument(FrameView* v)
{
    return new SVGDocument(this, v);
}

// vim:ts=4:noet
#endif // SVG_SUPPORT

