/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2005, 2006, 2008, 2012 Apple Inc. All rights reserved.
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

#if ENABLE(XSLT)

#include "CachedResourceLoader.h"
#include "DocumentInlines.h"
#include "FrameDestructionObserverInlines.h"
#include "LocalFrame.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "TransformSource.h"
#include "XMLDocumentParser.h"
#include "XMLDocumentParserScope.h"
#include "XSLImportRule.h"
#include "XSLTProcessor.h"
#include <libxml/uri.h>
#include <libxslt/xsltutils.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/HexNumber.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

XSLStyleSheet::XSLStyleSheet(XSLStyleSheet* parentSheet, const String& originalURL, const URL& finalURL)
    : m_ownerNode(nullptr)
    , m_originalURL(originalURL)
    , m_finalURL(finalURL)
    , m_embedded(false)
    , m_processed(false) // Child sheets get marked as processed when the libxslt engine has finally seen them.
    , m_parentStyleSheet(parentSheet)
{
}

XSLStyleSheet::XSLStyleSheet(Node* parentNode, const String& originalURL, const URL& finalURL,  bool embedded)
    : m_ownerNode(parentNode)
    , m_originalURL(originalURL)
    , m_finalURL(finalURL)
    , m_embedded(embedded)
    , m_processed(true) // The root sheet starts off processed.
{
}

XSLStyleSheet::~XSLStyleSheet()
{
    clearXSLStylesheetDocument();

    for (auto& import : m_children) {
        ASSERT(import->parentStyleSheet() == this);
        import->setParentStyleSheet(nullptr);
    }
}

bool XSLStyleSheet::isLoading() const
{
    for (auto& import : m_children) {
        if (import->isLoading())
            return true;
    }
    return false;
}

void XSLStyleSheet::checkLoaded()
{
    if (isLoading())
        return;
    if (RefPtr styleSheet = parentStyleSheet())
        styleSheet->checkLoaded();
    if (ownerNode())
        ownerNode()->sheetLoaded();
}

xmlDocPtr XSLStyleSheet::document()
{
    if (m_embedded && ownerDocument() && ownerDocument()->transformSource())
        return ownerDocument()->transformSource()->platformSource();
    return m_stylesheetDoc;
}

void XSLStyleSheet::clearDocuments()
{
    clearXSLStylesheetDocument();

    for (auto& import : m_children) {
        if (import->styleSheet())
            import->styleSheet()->clearDocuments();
    }
}

void XSLStyleSheet::clearXSLStylesheetDocument()
{
    if (!m_stylesheetDocTaken) {
        if (m_stylesheetDoc)
            xmlFreeDoc(m_stylesheetDoc);
    } else
        m_stylesheetDocTaken = false;
    m_stylesheetDoc = nullptr;
}

CachedResourceLoader* XSLStyleSheet::cachedResourceLoader()
{
    Document* document = ownerDocument();
    if (!document)
        return nullptr;
    return &document->cachedResourceLoader();
}

bool XSLStyleSheet::parseString(const String& string)
{
    // Parse in a single chunk into an xmlDocPtr
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char*>(&byteOrderMark);
    clearXSLStylesheetDocument();

    PageConsoleClient* console = nullptr;
    auto* frame = ownerDocument()->frame();
    if (frame && frame->page())
        console = &frame->page()->console();

    XMLDocumentParserScope scope(cachedResourceLoader(), XSLTProcessor::genericErrorFunc, XSLTProcessor::parseErrorFunc, console);

    auto upconvertedCharacters = StringView(string).upconvertedCharacters();
    const char* buffer = reinterpret_cast<const char*>(upconvertedCharacters.get());
    CheckedUint32 unsignedSize = string.length();
    unsignedSize *= sizeof(UChar);
    if (unsignedSize.hasOverflowed() || unsignedSize > static_cast<unsigned>(std::numeric_limits<int>::max()))
        return false;

    int size = unsignedSize.value<int>();
    xmlParserCtxtPtr ctxt = xmlCreateMemoryParserCtxt(buffer, size);
    if (!ctxt)
        return false;

    if (m_parentStyleSheet && m_parentStyleSheet->m_stylesheetDoc) {
        // The XSL transform may leave the newly-transformed document
        // with references to the symbol dictionaries of the style sheet
        // and any of its children. XML document disposal can corrupt memory
        // if a document uses more than one symbol dictionary, so we
        // ensure that all child stylesheets use the same dictionaries as their
        // parents.
        xmlDictFree(ctxt->dict);
        ctxt->dict = m_parentStyleSheet->m_stylesheetDoc->dict;
        xmlDictReference(ctxt->dict);
    }

    m_stylesheetDoc = xmlCtxtReadMemory(ctxt, buffer, size,
        finalURL().string().utf8().data(),
        BOMHighByte == 0xFF ? "UTF-16LE" : "UTF-16BE",
        XML_PARSE_NOENT | XML_PARSE_DTDATTR | XML_PARSE_NOWARNING | XML_PARSE_NOCDATA);
    xmlFreeParserCtxt(ctxt);

    loadChildSheets();

    return !!m_stylesheetDoc;
}

void XSLStyleSheet::loadChildSheets()
{
    if (!document())
        return;

    xmlNodePtr stylesheetRoot = document()->children;

    // Top level children may include other things such as DTD nodes, we ignore those.
    while (stylesheetRoot && stylesheetRoot->type != XML_ELEMENT_NODE)
        stylesheetRoot = stylesheetRoot->next;

    if (m_embedded) {
        // We have to locate (by ID) the appropriate embedded stylesheet element, so that we can walk the
        // import/include list.
        xmlAttrPtr idNode = xmlGetID(document(), (const xmlChar*)(finalURL().string().utf8().data()));
        if (!idNode)
            return;
        stylesheetRoot = idNode->parent;
    } else {
        // FIXME: Need to handle an external URI with a # in it.  This is a pretty minor edge case, so we'll deal
        // with it later.
    }

    if (stylesheetRoot) {
        // Walk the children of the root element and look for import/include elements.
        // Imports must occur first.
        xmlNodePtr curr = stylesheetRoot->children;
        while (curr) {
            if (curr->type != XML_ELEMENT_NODE) {
                curr = curr->next;
                continue;
            }
            if (IS_XSLT_ELEM(curr) && IS_XSLT_NAME(curr, "import")) {
                xmlChar* uriRef = xsltGetNsProp(curr, (const xmlChar*)"href", XSLT_NAMESPACE);
                loadChildSheet(String::fromUTF8((const char*)uriRef));
                xmlFree(uriRef);
            } else
                break;
            curr = curr->next;
        }

        // Now handle includes.
        while (curr) {
            if (curr->type == XML_ELEMENT_NODE && IS_XSLT_ELEM(curr) && IS_XSLT_NAME(curr, "include")) {
                xmlChar* uriRef = xsltGetNsProp(curr, (const xmlChar*)"href", XSLT_NAMESPACE);
                loadChildSheet(String::fromUTF8((const char*)uriRef));
                xmlFree(uriRef);
            }
            curr = curr->next;
        }
    }
}

void XSLStyleSheet::loadChildSheet(const String& href)
{
    auto childRule = makeUnique<XSLImportRule>(*this, href);
    m_children.append(childRule.release());
    m_children.last()->loadSheet();
}

xsltStylesheetPtr XSLStyleSheet::compileStyleSheet()
{
    // FIXME: Hook up error reporting for the stylesheet compilation process.
    if (m_embedded)
        return xsltLoadStylesheetPI(document());

    // Certain libxslt versions are corrupting the xmlDoc on compilation
    // failures - hence attempting to recompile after a failure is unsafe.
    if (m_compilationFailed)
        return nullptr;

    // xsltParseStylesheetDoc makes the document part of the stylesheet
    // so we have to release our pointer to it.
    ASSERT(m_stylesheetDoc);
    ASSERT(!m_stylesheetDocTaken);
    xsltStylesheetPtr result = xsltParseStylesheetDoc(m_stylesheetDoc);
    if (result)
        m_stylesheetDocTaken = true;
    else
        m_compilationFailed = true;
    return result;
}

void XSLStyleSheet::setParentStyleSheet(XSLStyleSheet* parent)
{
    m_parentStyleSheet = parent;
}

Document* XSLStyleSheet::ownerDocument()
{
    for (RefPtr styleSheet = this; styleSheet; styleSheet = styleSheet->parentStyleSheet()) {
        if (auto* node = styleSheet->ownerNode())
            return &node->document();
    }
    return nullptr;
}

xmlDocPtr XSLStyleSheet::locateStylesheetSubResource(xmlDocPtr parentDoc, const xmlChar* uri)
{
    bool matchedParent = (parentDoc == document());
    for (auto& import : m_children) {
        RefPtr child = import->styleSheet();
        if (!child)
            continue;
        if (matchedParent) {
            if (child->processed())
                continue; // libxslt has been given this sheet already.

            // Check the URI of the child stylesheet against the doc URI.
            // In order to ensure that libxml canonicalized both URLs, we get the original href
            // string from the import rule and canonicalize it using libxml before comparing it
            // with the URI argument.
            CString importHref = import->href().utf8();
            xmlChar* base = xmlNodeGetBase(parentDoc, (xmlNodePtr)parentDoc);
            xmlChar* childURI = xmlBuildURI((const xmlChar*)importHref.data(), base);
            bool equalURIs = xmlStrEqual(uri, childURI);
            xmlFree(base);
            xmlFree(childURI);
            if (equalURIs) {
                child->markAsProcessed();
                return child->document();
            }
            continue;
        }
        xmlDocPtr result = child->locateStylesheetSubResource(parentDoc, uri);
        if (result)
            return result;
    }

    return 0;
}

void XSLStyleSheet::markAsProcessed()
{
    ASSERT(!m_processed);
    ASSERT(!m_stylesheetDocTaken);
    m_processed = true;
    m_stylesheetDocTaken = true;
}

String XSLStyleSheet::debugDescription() const
{
    return makeString("XSLStyleSheet "_s, "0x"_s, hex(reinterpret_cast<uintptr_t>(this), Lowercase), ' ', href());
}

} // namespace WebCore

#endif // ENABLE(XSLT)
