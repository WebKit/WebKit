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
#include "DeprecatedGlobalSettings.h"
#include "DocumentType.h"
#include "Element.h"
#include "FTPDirectoryDocument.h"
#include "FragmentScriptingPermission.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "HTMLDocument.h"
#include "HTMLHeadElement.h"
#include "HTMLTitleElement.h"
#include "Image.h"
#include "ImageDocument.h"
#include "MIMETypeRegistry.h"
#include "MediaDocument.h"
#include "MediaPlayer.h"
#include "MediaQueryParser.h"
#include "PDFDocument.h"
#include "Page.h"
#include "PluginData.h"
#include "PluginDocument.h"
#include "SVGDocument.h"
#include "SVGNames.h"
#include "SecurityOrigin.h"
#include "SecurityOriginPolicy.h"
#include "Settings.h"
#include "StyleSheetContents.h"
#include "Text.h"
#include "TextDocument.h"
#include "XMLDocument.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(MODEL_ELEMENT)
#include "ModelDocument.h"
#endif

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(DOMImplementation);

DOMImplementation::DOMImplementation(Document& document)
    : m_document(document)
{
}

ExceptionOr<Ref<DocumentType>> DOMImplementation::createDocumentType(const AtomString& qualifiedName, const String& publicId, const String& systemId)
{
    auto parseResult = Document::parseQualifiedName(qualifiedName);
    if (parseResult.hasException())
        return parseResult.releaseException();
    return DocumentType::create(m_document, qualifiedName, publicId, systemId);
}

static inline Ref<XMLDocument> createXMLDocument(const String& namespaceURI, const Settings& settings)
{
    RefPtr<XMLDocument> document;
    if (namespaceURI == SVGNames::svgNamespaceURI)
        document = SVGDocument::create(nullptr, settings, URL());
    else if (namespaceURI == HTMLNames::xhtmlNamespaceURI)
        document = XMLDocument::createXHTML(nullptr, settings, URL());
    else
        document = XMLDocument::create(nullptr, settings, URL());
    document->setParserContentPolicy({ ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::AllowPluginContent });
    return document.releaseNonNull();
}

ExceptionOr<Ref<XMLDocument>> DOMImplementation::createDocument(const AtomString& namespaceURI, const AtomString& qualifiedName, DocumentType* documentType)
{
    auto document = createXMLDocument(namespaceURI, m_document.settings());
    document->setParserContentPolicy({ ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::AllowPluginContent });
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

    return document;
}

Ref<CSSStyleSheet> DOMImplementation::createCSSStyleSheet(const String&, const String& media)
{
    // FIXME: Title should be set.
    // FIXME: Media could have wrong syntax, in which case we should generate an exception.
    auto sheet = CSSStyleSheet::create(StyleSheetContents::create());
    sheet->setMediaQueries(MQ::MediaQueryParser::parse(media, { }));
    return sheet;
}

Ref<HTMLDocument> DOMImplementation::createHTMLDocument(String&& title)
{
    auto document = HTMLDocument::create(nullptr, m_document.settings(), URL(), { });
    document->setParserContentPolicy({ ParserContentPolicy::AllowScriptingContent, ParserContentPolicy::AllowPluginContent });
    document->open();
    document->write(nullptr, { "<!doctype html><html><head></head><body></body></html>"_s });
    if (!title.isNull()) {
        auto titleElement = HTMLTitleElement::create(titleTag, document);
        titleElement->appendChild(document->createTextNode(WTFMove(title)));
        ASSERT(document->head());
        document->head()->appendChild(titleElement);
    }
    document->setContextDocument(m_document.contextDocument());
    document->setSecurityOriginPolicy(m_document.securityOriginPolicy());
    return document;
}

Ref<Document> DOMImplementation::createDocument(const String& contentType, Frame* frame, const Settings& settings, const URL& url, ScriptExecutionContextIdentifier documentIdentifier)
{
    // FIXME: Inelegant to have this here just because this is the home of DOM APIs for creating documents.
    // This is internal, not a DOM API. Maybe we should put it in a new class called DocumentFactory,
    // because of the analogy with HTMLElementFactory.

    // Plug-ins cannot take over for HTML, XHTML, plain text, or non-PDF images.
    if (equalLettersIgnoringASCIICase(contentType, "text/html"_s))
        return HTMLDocument::create(frame, settings, url, documentIdentifier);
    if (equalLettersIgnoringASCIICase(contentType, "application/xhtml+xml"_s))
        return XMLDocument::createXHTML(frame, settings, url);
    if (equalLettersIgnoringASCIICase(contentType, "text/plain"_s))
        return TextDocument::create(frame, settings, url, documentIdentifier);

#if ENABLE(PDFJS)
    if (frame && settings.pdfJSViewerEnabled() && MIMETypeRegistry::isPDFMIMEType(contentType))
        return PDFDocument::create(*frame, url);
#endif

    bool isImage = MIMETypeRegistry::isSupportedImageMIMEType(contentType);
    if (frame && isImage && !MIMETypeRegistry::isPDFOrPostScriptMIMEType(contentType))
        return ImageDocument::create(*frame, url);

    // The "image documents for subframe PDFs" mode will override a PDF plug-in.
    if (frame && !frame->isMainFrame() && MIMETypeRegistry::isPDFMIMEType(contentType) && frame->settings().useImageDocumentForSubframePDF())
        return ImageDocument::create(*frame, url);

#if ENABLE(VIDEO)
    MediaEngineSupportParameters parameters;
    parameters.type = ContentType { contentType };
    parameters.url = url;
    if (MediaPlayer::supportsType(parameters) != MediaPlayer::SupportsType::IsNotSupported)
        return MediaDocument::create(frame, settings, url);
#endif

#if ENABLE(MODEL_ELEMENT)
    if (MIMETypeRegistry::isUSDMIMEType(contentType) && DeprecatedGlobalSettings::modelDocumentEnabled())
        return ModelDocument::create(frame, settings, url);
#endif

#if ENABLE(FTPDIR)
    if (equalLettersIgnoringASCIICase(contentType, "application/x-ftp-directory"_s))
        return FTPDirectoryDocument::create(frame, settings, url);
#endif

    if (frame && frame->loader().client().shouldAlwaysUsePluginDocument(contentType))
        return PluginDocument::create(*frame, url);

    // The following is the relatively costly lookup that requires initializing the plug-in database.
    if (frame && frame->page()) {
        auto allowedPluginTypes = frame->arePluginsEnabled()
            ? PluginData::AllPlugins : PluginData::OnlyApplicationPlugins;
        if (frame->page()->pluginData().supportsWebVisibleMimeType(contentType, allowedPluginTypes))
            return PluginDocument::create(*frame, url);
    }

    // Items listed here, after the plug-in checks, can be overridden by plug-ins.
    // For example, plug-ins can take over support for PDF or SVG.
    if (frame && isImage)
        return ImageDocument::create(*frame, url);
    if (MIMETypeRegistry::isTextMIMEType(contentType))
        return TextDocument::create(frame, settings, url, documentIdentifier);
    if (equalLettersIgnoringASCIICase(contentType, "image/svg+xml"_s))
        return SVGDocument::create(frame, settings, url);
    if (MIMETypeRegistry::isXMLMIMEType(contentType)) {
        auto document = XMLDocument::create(frame, settings, url);
        document->overrideMIMEType(contentType);
        return document;
    }

    return HTMLDocument::create(frame, settings, url, documentIdentifier);
}

}
