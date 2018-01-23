/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "ContentType.h"
#include "DocumentType.h"
#include "Element.h"
#include "FTPDirectoryDocument.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLDocument.h"
#include "HTMLHeadElement.h"
#include "HTMLTitleElement.h"
#include "Image.h"
#include "ImageDocument.h"
#include "MIMETypeRegistry.h"
#include "MainFrame.h"
#include "MediaDocument.h"
#include "MediaList.h"
#include "MediaPlayer.h"
#include "Page.h"
#include "PluginData.h"
#include "PluginDocument.h"
#include "SVGDocument.h"
#include "SVGNames.h"
#include "SecurityOrigin.h"
#include "SecurityOriginPolicy.h"
#include "Settings.h"
#include "StyleSheetContents.h"
#include "SubframeLoader.h"
#include "Text.h"
#include "TextDocument.h"
#include "XMLDocument.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

DOMImplementation::DOMImplementation(Document& document)
    : m_document(document)
{
}

ExceptionOr<Ref<DocumentType>> DOMImplementation::createDocumentType(const String& qualifiedName, const String& publicId, const String& systemId)
{
    auto parseResult = Document::parseQualifiedName(qualifiedName);
    if (parseResult.hasException())
        return parseResult.releaseException();
    return DocumentType::create(m_document, qualifiedName, publicId, systemId);
}

static inline Ref<XMLDocument> createXMLDocument(const String& namespaceURI)
{
    if (namespaceURI == SVGNames::svgNamespaceURI)
        return SVGDocument::create(nullptr, URL());
    if (namespaceURI == HTMLNames::xhtmlNamespaceURI)
        return XMLDocument::createXHTML(nullptr, URL());
    return XMLDocument::create(nullptr, URL());
}

ExceptionOr<Ref<XMLDocument>> DOMImplementation::createDocument(const String& namespaceURI, const String& qualifiedName, DocumentType* documentType)
{
    auto document = createXMLDocument(namespaceURI);
    document->setContextDocument(m_document.contextDocument());
    document->setSecurityOriginPolicy(m_document.securityOriginPolicy());

    RefPtr<Element> documentElement;
    if (!qualifiedName.isEmpty()) {
        ASSERT(!document->domWindow()); // If domWindow is not null, createElementNS could find CustomElementRegistry and arbitrary scripts.
        auto result = document->createElementNS(namespaceURI, qualifiedName);
        if (result.hasException())
            return result.releaseException();
        documentElement = result.releaseReturnValue();
    }

    if (documentType)
        document->appendChild(*documentType);
    if (documentElement)
        document->appendChild(*documentElement);

    return WTFMove(document);
}

Ref<CSSStyleSheet> DOMImplementation::createCSSStyleSheet(const String&, const String& media)
{
    // FIXME: Title should be set.
    // FIXME: Media could have wrong syntax, in which case we should generate an exception.
    auto sheet = CSSStyleSheet::create(StyleSheetContents::create());
    sheet->setMediaQueries(MediaQuerySet::create(media));
    return sheet;
}

Ref<HTMLDocument> DOMImplementation::createHTMLDocument(const String& title)
{
    auto document = HTMLDocument::create(nullptr, URL());
    document->open();
    document->write(nullptr, { ASCIILiteral("<!doctype html><html><head></head><body></body></html>") });
    if (!title.isNull()) {
        auto titleElement = HTMLTitleElement::create(titleTag, document);
        titleElement->appendChild(document->createTextNode(title));
        ASSERT(document->head());
        document->head()->appendChild(titleElement);
    }
    document->setContextDocument(m_document.contextDocument());
    document->setSecurityOriginPolicy(m_document.securityOriginPolicy());
    return document;
}

Ref<Document> DOMImplementation::createDocument(const String& type, Frame* frame, const URL& url)
{
    // FIXME: Confusing to have this here with public DOM APIs for creating documents. This is different enough that it should perhaps be moved.
    // FIXME: This function is doing case insensitive comparisons on MIME types. Should do equalLettersIgnoringASCIICase instead.

    // Plugins cannot take HTML and XHTML from us, and we don't even need to initialize the plugin database for those.
    if (type == "text/html")
        return HTMLDocument::create(frame, url);
    if (type == "application/xhtml+xml")
        return XMLDocument::createXHTML(frame, url);

#if ENABLE(FTPDIR)
    // Plugins cannot take FTP from us either
    if (type == "application/x-ftp-directory")
        return FTPDirectoryDocument::create(frame, url);
#endif

    // If we want to useImageDocumentForSubframePDF, we'll let that override plugin support.
    if (frame && !frame->isMainFrame() && MIMETypeRegistry::isPDFMIMEType(type) && frame->settings().useImageDocumentForSubframePDF())
        return ImageDocument::create(*frame, url);

    PluginData* pluginData = nullptr;
    auto allowedPluginTypes = PluginData::OnlyApplicationPlugins;
    if (frame && frame->page()) {
        if (frame->loader().subframeLoader().allowPlugins())
            allowedPluginTypes = PluginData::AllPlugins;

        pluginData = &frame->page()->pluginData();
    }

    // PDF is one image type for which a plugin can override built-in support.
    // We do not want QuickTime to take over all image types, obviously.
    if (MIMETypeRegistry::isPDFOrPostScriptMIMEType(type) && pluginData && pluginData->supportsWebVisibleMimeType(type, allowedPluginTypes))
        return PluginDocument::create(frame, url);
    if (Image::supportsType(type))
        return ImageDocument::create(*frame, url);

#if ENABLE(VIDEO)
    // Check to see if the type can be played by our MediaPlayer, if so create a MediaDocument
    // Key system is not applicable here.
    MediaEngineSupportParameters parameters;
    parameters.type = ContentType(type);
    parameters.url = url;
    if (MediaPlayer::supportsType(parameters))
        return MediaDocument::create(frame, url);
#endif

    // Everything else except text/plain can be overridden by plugins. In particular, Adobe SVG Viewer should be used for SVG, if installed.
    // Disallowing plug-ins to use text/plain prevents plug-ins from hijacking a fundamental type that the browser is expected to handle,
    // and also serves as an optimization to prevent loading the plug-in database in the common case.
    if (type != "text/plain" && ((pluginData && pluginData->supportsWebVisibleMimeType(type, allowedPluginTypes)) || (frame && frame->loader().client().shouldAlwaysUsePluginDocument(type))))
        return PluginDocument::create(frame, url);
    if (MIMETypeRegistry::isTextMIMEType(type))
        return TextDocument::create(frame, url);

    if (type == "image/svg+xml")
        return SVGDocument::create(frame, url);

    if (MIMETypeRegistry::isXMLMIMEType(type))
        return XMLDocument::create(frame, url);

    return HTMLDocument::create(frame, url);
}

}
