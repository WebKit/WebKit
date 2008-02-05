/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
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
#include "DOMImplementation.h"

#include "CSSStyleSheet.h"
#include "DocumentType.h"
#include "Element.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FTPDirectoryDocument.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "HTMLViewSourceDocument.h"
#include "Image.h"
#include "ImageDocument.h"
#include "MediaList.h"
#include "MIMETypeRegistry.h"
#include "Page.h"
#include "PluginDocument.h"
#include "PluginInfoStore.h"
#include "RegularExpression.h"
#include "Settings.h"
#include "TextDocument.h"
#include "XMLNames.h"

#if ENABLE(SVG)
#include "SVGNames.h"
#include "SVGDocument.h"
#endif

namespace WebCore {

// FIXME: An implementation of this is still waiting for me to understand the distinction between
// a "malformed" qualified name and one with bad characters in it. For example, is a second colon
// an illegal character or a malformed qualified name? This will determine both what parameters
// this function needs to take and exactly what it will do. Should also be exported so that
// Element can use it too.
static bool qualifiedNameIsMalformed(const String&)
{
    return false;
}

#if ENABLE(SVG)

static void addString(HashSet<String, CaseFoldingHash>& set, const char* string)
{
    set.add(string);
}

static bool isSVG10Feature(const String &feature)
{
    static bool initialized = false;
    static HashSet<String, CaseFoldingHash> svgFeatures;
    if (!initialized) {
#if ENABLE(SVG_USE) && ENABLE(SVG_FOREIGN_OBJECT) && ENABLE(SVG_FILTER) && ENABLE(SVG_FONTS)
        addString(svgFeatures, "svg");
        addString(svgFeatures, "svg.static");
#endif
//      addString(svgFeatures, "svg.animation");
//      addString(svgFeatures, "svg.dynamic");
//      addString(svgFeatures, "svg.dom.animation");
//      addString(svgFeatures, "svg.dom.dynamic");
#if ENABLE(SVG_USE) && ENABLE(SVG_FOREIGN_OBJECT) && ENABLE(SVG_FILTER) && ENABLE(SVG_FONTS)
        addString(svgFeatures, "dom");
        addString(svgFeatures, "dom.svg");
        addString(svgFeatures, "dom.svg.static");
#endif
//      addString(svgFeatures, "svg.all");
//      addString(svgFeatures, "dom.svg.all");
        initialized = true;
    }
    return svgFeatures.contains(feature);
}

static bool isSVG11Feature(const String &feature)
{
    static bool initialized = false;
    static HashSet<String, CaseFoldingHash> svgFeatures;
    if (!initialized) {
        // Sadly, we cannot claim to implement any of the SVG 1.1 generic feature sets
        // lack of Font and Filter support.
        // http://bugs.webkit.org/show_bug.cgi?id=15480
#if ENABLE(SVG_USE) && ENABLE(SVG_FOREIGN_OBJECT) && ENABLE(SVG_FILTER) && ENABLE(SVG_FONTS)
        addString(svgFeatures, "SVG");
        addString(svgFeatures, "SVGDOM");
        addString(svgFeatures, "SVG-static");
        addString(svgFeatures, "SVGDOM-static");
#endif
//      addString(svgFeatures, "SVG-animation);
//      addString(svgFeatures, "SVGDOM-animation);
//      addString(svgFeatures, "SVG-dynamic);
//      addString(svgFeatures, "SVGDOM-dynamic);
        addString(svgFeatures, "CoreAttribute");
#if ENABLE(SVG_USE)
        addString(svgFeatures, "Structure");
        addString(svgFeatures, "BasicStructure");
#endif
        addString(svgFeatures, "ContainerAttribute");
        addString(svgFeatures, "ConditionalProcessing");
        addString(svgFeatures, "Image");
        addString(svgFeatures, "Style");
        addString(svgFeatures, "ViewportAttribute");
        addString(svgFeatures, "Shape");
//      addString(svgFeatures, "Text"); // requires altGlyph, bug 6426
        addString(svgFeatures, "BasicText");
        addString(svgFeatures, "PaintAttribute");
        addString(svgFeatures, "BasicPaintAttribute");
        addString(svgFeatures, "OpacityAttribute");
        addString(svgFeatures, "GraphicsAttribute");
        addString(svgFeatures, "BaseGraphicsAttribute");
        addString(svgFeatures, "Marker");
//      addString(svgFeatures, "ColorProfile"); // requires color-profile, bug 6037
        addString(svgFeatures, "Gradient");
        addString(svgFeatures, "Pattern");
        addString(svgFeatures, "Clip");
        addString(svgFeatures, "BasicClip");
        addString(svgFeatures, "Mask");
#if ENABLE(SVG_FILTER)
//      addString(svgFeatures, "Filter");
        addString(svgFeatures, "BasicFilter");
#endif
        addString(svgFeatures, "DocumentEventsAttribute");
        addString(svgFeatures, "GraphicalEventsAttribute");
//      addString(svgFeatures, "AnimationEventsAttribute");
        addString(svgFeatures, "Cursor");
        addString(svgFeatures, "Hyperlinking");
        addString(svgFeatures, "XlinkAttribute");
        addString(svgFeatures, "ExternalResourcesRequired");
//      addString(svgFeatures, "View"); // buggy <view> support, bug 16962
        addString(svgFeatures, "Script");
//      addString(svgFeatures, "Animation"); // <animate> support missing
#if ENABLE(SVG_FONTS)
        addString(svgFeatures, "Font");
        addString(svgFeatures, "BasicFont");
#endif
#if ENABLE(SVG_FOREIGN_OBJECT)
        addString(svgFeatures, "Extensibility");
#endif
        initialized = true;
    }
    return svgFeatures.contains(feature);
}
#endif

DOMImplementation::~DOMImplementation()
{
}

bool DOMImplementation::hasFeature (const String& feature, const String& version) const
{
    String lower = feature.lower();
    if (lower == "core" || lower == "html" || lower == "xml" || lower == "xhtml")
        return version.isEmpty() || version == "1.0" || version == "2.0";
    if (lower == "css"
            || lower == "css2"
            || lower == "events"
            || lower == "htmlevents"
            || lower == "mouseevents"
            || lower == "mutationevents"
            || lower == "range"
            || lower == "stylesheets"
            || lower == "traversal"
            || lower == "uievents"
            || lower == "views")
        return version.isEmpty() || version == "2.0";
    if (lower == "xpath" || lower == "textevents")
        return version.isEmpty() || version == "3.0";

#if ENABLE(SVG)
    if ((version.isEmpty() || version == "1.1") && feature.startsWith("http://www.w3.org/tr/svg11/feature#", false)) {
        if (isSVG11Feature(feature.right(feature.length() - 35)))
            return true;
    }

    if ((version.isEmpty() || version == "1.0") && feature.startsWith("org.w3c.", false)) {
        if (isSVG10Feature(feature.right(feature.length() - 8)))
            return true;
    }
#endif
    
    return false;
}

PassRefPtr<DocumentType> DOMImplementation::createDocumentType(const String& qualifiedName,
    const String& publicId, const String& systemId, ExceptionCode& ec)
{
    // Not mentioned in spec: throw NAMESPACE_ERR if no qualifiedName supplied
    if (qualifiedName.isNull()) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
    String prefix, localName;
    if (!Document::parseQualifiedName(qualifiedName, prefix, localName)) {
        ec = INVALID_CHARACTER_ERR;
        return 0;
    }

    // NAMESPACE_ERR: Raised if the qualifiedName is malformed.
    if (qualifiedNameIsMalformed(qualifiedName)) {
        ec = NAMESPACE_ERR;
        return 0;
    }

    ec = 0;
    return new DocumentType(this, 0, qualifiedName, publicId, systemId);
}

DOMImplementation* DOMImplementation::getInterface(const String& /*feature*/) const
{
    // ###
    return 0;
}

PassRefPtr<Document> DOMImplementation::createDocument(const String& namespaceURI,
    const String& qualifiedName, DocumentType* doctype, ExceptionCode& ec)
{
    if (!qualifiedName.isEmpty()) {
        // INVALID_CHARACTER_ERR: Raised if the specified qualified name contains an illegal character.
        String prefix, localName;
        if (!Document::parseQualifiedName(qualifiedName, prefix, localName)) {
            ec = INVALID_CHARACTER_ERR;
            return 0;
        }

        // NAMESPACE_ERR:
        // - Raised if the qualifiedName is malformed,
        // - if the qualifiedName has a prefix and the namespaceURI is null, or
        // - if the qualifiedName has a prefix that is "xml" and the namespaceURI is different
        //   from "http://www.w3.org/XML/1998/namespace" [Namespaces].
        int colonpos = qualifiedName.find(':');    
        if (qualifiedNameIsMalformed(qualifiedName) ||
            (colonpos >= 0 && namespaceURI.isNull()) ||
            (colonpos == 3 && qualifiedName[0] == 'x' && qualifiedName[1] == 'm' && qualifiedName[2] == 'l' &&
#if ENABLE(SVG)
             namespaceURI != SVGNames::svgNamespaceURI &&
#endif
             namespaceURI != XMLNames::xmlNamespaceURI)) {

            ec = NAMESPACE_ERR;
            return 0;
        }
    }
    
    // WRONG_DOCUMENT_ERR: Raised if doctype has already been used with a different document or was
    // created from a different implementation.
    if (doctype && (doctype->document() || doctype->implementation() != this)) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    RefPtr<Document> doc;
#if ENABLE(SVG)
    if (namespaceURI == SVGNames::svgNamespaceURI)
        doc = new SVGDocument(this, 0);
    else
#endif
        if (namespaceURI == HTMLNames::xhtmlNamespaceURI)
            doc = new Document(this, 0, true);
        else
            doc = new Document(this, 0);

    // now get the interesting parts of the doctype
    if (doctype) {
        doc->setDocType(doctype);
        doctype->setDocument(doc.get());
    }

    if (!qualifiedName.isEmpty())
        doc->addChild(doc->createElementNS(namespaceURI, qualifiedName, ec));
    
    ec = 0;
    return doc.release();
}

PassRefPtr<CSSStyleSheet> DOMImplementation::createCSSStyleSheet(const String&, const String& media, ExceptionCode& ec)
{
    // ### TODO : title should be set, and media could have wrong syntax, in which case we should generate an exception.
    ec = 0;
    CSSStyleSheet* const nullSheet = 0;
    RefPtr<CSSStyleSheet> sheet = new CSSStyleSheet(nullSheet);
    sheet->setMedia(new MediaList(sheet.get(), media, true));
    return sheet.release();
}

PassRefPtr<Document> DOMImplementation::createDocument(Frame* frame)
{
    return new Document(this, frame);
}

PassRefPtr<HTMLDocument> DOMImplementation::createHTMLDocument(Frame* frame)
{
    return new HTMLDocument(this, frame);
}

DOMImplementation* DOMImplementation::instance()
{
    static RefPtr<DOMImplementation> i = new DOMImplementation;
    return i.get();
}

bool DOMImplementation::isXMLMIMEType(const String& mimeType)
{
    if (mimeType == "text/xml" || mimeType == "application/xml" || mimeType == "text/xsl")
        return true;
    static const char* validChars = "[0-9a-zA-Z_\\-+~!$\\^{}|.%'`#&*]"; // per RFCs: 3023, 2045
    static RegularExpression xmlTypeRegExp(DeprecatedString("^") + validChars + "+/" + validChars + "+\\+xml$");
    if (xmlTypeRegExp.match(mimeType.deprecatedString()) > -1)
        return true;
    return false;
}

bool DOMImplementation::isTextMIMEType(const String& mimeType)
{
    if (MIMETypeRegistry::isSupportedJavaScriptMIMEType(mimeType) ||
        (mimeType.startsWith("text/") && mimeType != "text/html" &&
         mimeType != "text/xml" && mimeType != "text/xsl"))
        return true;

    return false;
}

PassRefPtr<HTMLDocument> DOMImplementation::createHTMLDocument(const String& title)
{
    RefPtr<HTMLDocument> d = new HTMLDocument(this, 0);
    d->open();
    d->write("<html><head><title>" + title + "</title></head><body></body></html>");
    return d.release();
}

PassRefPtr<Document> DOMImplementation::createDocument(const String& type, Frame* frame, bool inViewSourceMode)
{
    if (inViewSourceMode) {
        if (type == "text/html" || type == "application/xhtml+xml" || type == "image/svg+xml" || isTextMIMEType(type) || isXMLMIMEType(type))
            return new HTMLViewSourceDocument(this, frame, type);
    }

    // Plugins cannot take HTML and XHTML from us, and we don't even need to initialize the plugin database for those.
    if (type == "text/html")
        return new HTMLDocument(this, frame);
    if (type == "application/xhtml+xml")
        return new Document(this, frame, true);
        
#if ENABLE(FTPDIR)
    // Plugins cannot take FTP from us either
    if (type == "application/x-ftp-directory")
        return new FTPDirectoryDocument(this, frame);
#endif

    // PDF is one image type for which a plugin can override built-in support.
    // We do not want QuickTime to take over all image types, obviously.
    if ((type == "application/pdf" || type == "text/pdf") && PluginInfoStore::supportsMIMEType(type))
        return new PluginDocument(this, frame);
    if (Image::supportsType(type))
        return new ImageDocument(this, frame);
    // Everything else except text/plain can be overridden by plugins. In particular, Adobe SVG Viewer should be used for SVG, if installed.
    // Disallowing plug-ins to use text/plain prevents plug-ins from hijacking a fundamental type that the browser is expected to handle,
    // and also serves as an optimization to prevent loading the plug-in database in the common case.
    if (type != "text/plain" && PluginInfoStore::supportsMIMEType(type)) 
        return new PluginDocument(this, frame);
    if (isTextMIMEType(type))
        return new TextDocument(this, frame);

#if ENABLE(SVG)
    if (type == "image/svg+xml") {
        Settings* settings = frame ? frame->settings() : 0;
        if (!settings || !settings->usesDashboardBackwardCompatibilityMode())
            return new SVGDocument(this, frame);
    }
#endif
    if (isXMLMIMEType(type))
        return new Document(this, frame);

    return new HTMLDocument(this, frame);
}

}
