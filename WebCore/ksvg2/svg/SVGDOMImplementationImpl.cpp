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
#include "SVGDOMImplementationImpl.h"

#include <kdom/Helper.h>
#include <kdom/Namespace.h>
#include <kdom/core/DOMExceptionImpl.h>
#include <kdom/core/ElementImpl.h>
#include <kdom/kdom.h>

#include "DocumentImpl.h"
#include "DocumentTypeImpl.h"
#include "ExceptionCode.h"
#include "PlatformString.h"
#include "SVGDocumentImpl.h"
#include "SVGRenderStyle.h"
#include "SVGSVGElementImpl.h"
#include "css_stylesheetimpl.h"
#include "ksvg.h"

using namespace WebCore;

SVGDOMImplementationImpl *SVGDOMImplementationImpl::s_instance = 0;
QStringList SVGDOMImplementationImpl::s_features;

SVGDOMImplementationImpl::SVGDOMImplementationImpl() : DOMImplementationImpl()
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

bool SVGDOMImplementationImpl::hasFeature(DOMStringImpl *featureImpl, DOMStringImpl *versionImpl) const
{
    QString feature = (featureImpl ? DOMString(featureImpl).upper().qstring() : QString::null);
    DOMString version(versionImpl);

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

    return DOMImplementationImpl::hasFeature(featureImpl, versionImpl);
}

PassRefPtr<DocumentTypeImpl> SVGDOMImplementationImpl::createDocumentType(DOMStringImpl *qualifiedNameImpl, DOMStringImpl *publicId, DOMStringImpl *systemId, ExceptionCode& ec) const
{
    DOMString qualifiedName(qualifiedNameImpl);
#if 0
    // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
    if(!qualifiedName.isEmpty() && !Helper::ValidateAttributeName(qualifiedNameImpl)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }

    // NAMESPACE_ERR: Raised if no qualifiedName supplied (not mentioned in the spec!)
    if(qualifiedName.isEmpty()) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    // NAMESPACE_ERR: Raised if the qualifiedName is malformed.
    Helper::CheckMalformedQualifiedName(qualifiedNameImpl);
#endif

    return new DocumentTypeImpl(0, qualifiedName, publicId, systemId);
}

PassRefPtr<DocumentImpl> SVGDOMImplementationImpl::createDocument(DOMStringImpl *namespaceURI, DOMStringImpl *qualifiedNameImpl, DocumentTypeImpl *doctype, ExceptionCode& ec) const
{
    return createDocument(namespaceURI, qualifiedNameImpl, doctype, true, 0, ec);
}

PassRefPtr<DocumentImpl> SVGDOMImplementationImpl::createDocument(DOMStringImpl *namespaceURIImpl, DOMStringImpl *qualifiedNameImpl, DocumentTypeImpl *doctype, bool createDocElement, KDOMView *view, ExceptionCode& ec) const
{
    DOMString namespaceURI(namespaceURIImpl);
    DOMString qualifiedName(qualifiedNameImpl);
    if((namespaceURI != SVGNames::svgNamespaceURI) || (qualifiedName != "svg" && qualifiedName != "svg:svg"))
        return DOMImplementationImpl::instance()->createDocument(namespaceURIImpl, qualifiedNameImpl, doctype, ec);

#if 0
    int dummy;
    Helper::CheckQualifiedName(qualifiedNameImpl, namespaceURIImpl, dummy, true /*nameCanBeNull*/, true /*nameCanBeEmpty, see #61650*/);
#endif

    // WRONG_DOCUMENT_ERR: Raised if docType has already been used with a different
    //                     document or was created from a different implementation.
    if(doctype != 0 && doctype->ownerDocument() != 0) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    RefPtr<SVGDocumentImpl> doc = new SVGDocumentImpl(const_cast<SVGDOMImplementationImpl *>(this), view);

    // TODO : what to do when doctype is null?
    if (doctype)
        doc->setDocType(doctype);

    // Add root element...
    if (createDocElement)
        doc->appendChild(doc->createElementNS(namespaceURI, qualifiedName, ec), ec);

    return doc.release();
}

PassRefPtr<CSSStyleSheetImpl> SVGDOMImplementationImpl::createCSSStyleSheet(DOMStringImpl *title, DOMStringImpl *media) const
{
    // TODO : check whether media is valid
    CSSStyleSheetImpl *parent = 0;
    RefPtr<CSSStyleSheetImpl> sheet = new CSSStyleSheetImpl(parent);
    //sheet->setTitle(title);
    sheet->setMedia(new MediaListImpl(sheet.get(), media));
    return sheet.release();
}

DocumentTypeImpl *SVGDOMImplementationImpl::defaultDocumentType() const
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
#endif // SVG_SUPPORT

