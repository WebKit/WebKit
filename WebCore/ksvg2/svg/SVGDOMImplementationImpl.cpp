/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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

#include <kdom/kdom.h>
#include <kdom/Helper.h>
#include <kdom/DOMString.h>
#include <kdom/Namespace.h>
#include <kdom/core/ElementImpl.h>
#include <kdom/core/DocumentImpl.h>
#include <kdom/core/DOMExceptionImpl.h>
#include <kdom/core/DocumentTypeImpl.h>
#include <kdom/css/MediaListImpl.h>
#include <kdom/css/CSSStyleSheetImpl.h>

#include "ksvg.h"
#include "SVGRenderStyle.h"
#include "SVGDocumentImpl.h"
#include "SVGSVGElementImpl.h"
#include "SVGDOMImplementationImpl.h"

using namespace KSVG;

SVGDOMImplementationImpl *SVGDOMImplementationImpl::s_instance = 0;
QStringList SVGDOMImplementationImpl::s_features;

SVGDOMImplementationImpl::SVGDOMImplementationImpl() : KDOM::DOMImplementationImpl()
{
    m_animationContext = false;
}

SVGDOMImplementationImpl::~SVGDOMImplementationImpl()
{
    // clean up static data
    //SVGRenderStyle::cleanup();
}

SVGDOMImplementationImpl *SVGDOMImplementationImpl::self()
{
    if(!s_instance)
    {
        s_instance = new SVGDOMImplementationImpl();
        
        // 1.1 features
        s_features.append(QString::fromLatin1("SVG"));
        s_features.append(QString::fromLatin1("SVGDOM"));
        s_features.append(QString::fromLatin1("SVG-STATIC"));
        s_features.append(QString::fromLatin1("SVGDOM-STATIC"));
        s_features.append(QString::fromLatin1("SVG-ANIMATION"));
        s_features.append(QString::fromLatin1("SVGDOM-ANIMATION"));
        s_features.append(QString::fromLatin1("SVG-DYNAMIC"));
        s_features.append(QString::fromLatin1("COREATTRIBUTE"));
        s_features.append(QString::fromLatin1("STRUCTURE"));
        s_features.append(QString::fromLatin1("CONTAINERATTRIBUTE"));
        s_features.append(QString::fromLatin1("CONDITIOANLPROCESSING"));
        s_features.append(QString::fromLatin1("IMAGE"));
        s_features.append(QString::fromLatin1("STYLE"));
        s_features.append(QString::fromLatin1("VIEWPORTATTRIBUTE"));
        s_features.append(QString::fromLatin1("SHAPE"));
        s_features.append(QString::fromLatin1("TEXT"));
        s_features.append(QString::fromLatin1("PAINTATTRIBUTE"));
        s_features.append(QString::fromLatin1("OPACITYATTRIBUTE"));
        s_features.append(QString::fromLatin1("GRAPHICSATTRIBUTE"));
        s_features.append(QString::fromLatin1("MARKER"));
        s_features.append(QString::fromLatin1("COLORPROFILE"));
        s_features.append(QString::fromLatin1("GRADIENT"));
        s_features.append(QString::fromLatin1("PATTERN"));
        s_features.append(QString::fromLatin1("CLIP"));
        s_features.append(QString::fromLatin1("MASK"));
        s_features.append(QString::fromLatin1("FILTER"));
        s_features.append(QString::fromLatin1("XLINKATTRIBUTE"));
        s_features.append(QString::fromLatin1("FONT"));
        s_features.append(QString::fromLatin1("EXTENSIBILITY"));

        // 1.0 features
        //s_features.append(QString::fromLatin1("SVG"));
        s_features.append(QString::fromLatin1("SVG.STATIC"));
        s_features.append(QString::fromLatin1("SVG.ANIMATION"));
        s_features.append(QString::fromLatin1("SVG.DYNAMIC"));
        s_features.append(QString::fromLatin1("DOM"));
        s_features.append(QString::fromLatin1("DOM.SVG"));
        s_features.append(QString::fromLatin1("DOM.SVG.STATIC"));
        s_features.append(QString::fromLatin1("DOM.SVG.ANIMATION"));
        s_features.append(QString::fromLatin1("DOM.SVG.DYNAMIC"));
        s_features.append(QString::fromLatin1("SVG.ALL"));
        s_features.append(QString::fromLatin1("DOM.SVG.ALL"));
    }

    return s_instance;
}

bool SVGDOMImplementationImpl::hasFeature(KDOM::DOMStringImpl *featureImpl, KDOM::DOMStringImpl *versionImpl) const
{
    QString feature = (featureImpl ? KDOM::DOMString(featureImpl).upper().qstring() : QString::null);
    KDOM::DOMString version(versionImpl);

    if((version.isEmpty() || version == "1.1") &&
       feature.startsWith(QString::fromLatin1("HTTP://WWW.W3.ORG/TR/SVG11/FEATURE#")))
    {
        if(s_features.contains(feature.right(feature.length() - 35)))
            return true;
    }

    if((version.isEmpty() || version == "1.0") &&
       feature.startsWith(QString::fromLatin1("ORG.W3C.")))
    {
        if(s_features.contains(feature.right(feature.length() - 8)))
            return true;
    }

    return KDOM::DOMImplementationImpl::hasFeature(featureImpl, versionImpl);
}

KDOM::DocumentTypeImpl *SVGDOMImplementationImpl::createDocumentType(KDOM::DOMStringImpl *qualifiedNameImpl, KDOM::DOMStringImpl *publicId, KDOM::DOMStringImpl *systemId, int& exceptioncode) const
{
    KDOM::DOMString qualifiedName(qualifiedNameImpl);
#if 0
    // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
    if(!qualifiedName.isEmpty() && !KDOM::Helper::ValidateAttributeName(qualifiedNameImpl)) {
        exceptioncode = KDOM::INVALID_CHARACTER_ERR;
        return 0;
    }

    // NAMESPACE_ERR: Raised if no qualifiedName supplied (not mentioned in the spec!)
    if(qualifiedName.isEmpty()) {
        exceptioncode = KDOM::NAMESPACE_ERR;
        return 0;
    }

    // NAMESPACE_ERR: Raised if the qualifiedName is malformed.
    KDOM::Helper::CheckMalformedQualifiedName(qualifiedNameImpl);
#endif

    return new KDOM::DocumentTypeImpl(0, qualifiedName, publicId, systemId);
}

KDOM::DocumentImpl *SVGDOMImplementationImpl::createDocument(KDOM::DOMStringImpl *namespaceURI, KDOM::DOMStringImpl *qualifiedNameImpl, KDOM::DocumentTypeImpl *doctype, int& exceptioncode) const
{
    return createDocument(namespaceURI, qualifiedNameImpl, doctype, true, 0, exceptioncode);
}

KDOM::DocumentImpl *SVGDOMImplementationImpl::createDocument(KDOM::DOMStringImpl *namespaceURIImpl, KDOM::DOMStringImpl *qualifiedNameImpl, KDOM::DocumentTypeImpl *doctype, bool createDocElement, KDOM::KDOMView *view, int& exceptioncode) const
{
    KDOM::DOMString namespaceURI(namespaceURIImpl);
    KDOM::DOMString qualifiedName(qualifiedNameImpl);
    if((namespaceURI != SVGNames::svgNamespaceURI) || (qualifiedName != "svg" && qualifiedName != "svg:svg"))
        return KDOM::DOMImplementationImpl::instance()->createDocument(namespaceURIImpl, qualifiedNameImpl, doctype, exceptioncode);

#if 0
    int dummy;
    KDOM::Helper::CheckQualifiedName(qualifiedNameImpl, namespaceURIImpl, dummy, true /*nameCanBeNull*/, true /*nameCanBeEmpty, see #61650*/);
#endif

    // WRONG_DOCUMENT_ERR: Raised if docType has already been used with a different
    //                     document or was created from a different implementation.
    if(doctype != 0 && doctype->ownerDocument() != 0) {
        exceptioncode = KDOM::WRONG_DOCUMENT_ERR;
        return 0;
    }

    SVGDocumentImpl *doc = new SVGDocumentImpl(const_cast<SVGDOMImplementationImpl *>(this), view);

    // TODO : what to do when doctype is null?
    if(doctype)
        doc->setDocType(doctype);

    // Add root element...
    if(createDocElement)
    {
        KDOM::ElementImpl *svg = doc->createElementNS(namespaceURI, qualifiedName, exceptioncode);
        doc->appendChild(svg, exceptioncode);
    }

    return doc;
}

KDOM::CSSStyleSheetImpl *SVGDOMImplementationImpl::createCSSStyleSheet(KDOM::DOMStringImpl *title, KDOM::DOMStringImpl *media) const
{
    // TODO : check whether media is valid
    KDOM::CSSStyleSheetImpl *parent = 0;
    KDOM::CSSStyleSheetImpl *sheet = new KDOM::CSSStyleSheetImpl(parent);
    //sheet->setTitle(title);
    sheet->setMedia(new KDOM::MediaListImpl(sheet, media));
    return sheet;
}

KDOM::DocumentTypeImpl *SVGDOMImplementationImpl::defaultDocumentType() const
{
    return 0;
    /*
    return createDocumentType("svg:svg", "-//W3C//DTD SVG20010904//EN",
                             "http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd");
    */
}

bool SVGDOMImplementationImpl::inAnimationContext() const
{
    return m_animationContext;
}

void SVGDOMImplementationImpl::setAnimationContext(bool value)
{
    m_animationContext = value;
}

// vim:ts=4:noet
