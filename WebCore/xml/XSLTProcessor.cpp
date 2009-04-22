/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple, Inc. All rights reserved.
 * Copyright (C) 2005, 2006 Alexey Proskuryakov <ap@webkit.org>
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

#include "XSLTProcessor.h"

#include "CString.h"
#include "Console.h"
#include "DOMImplementation.h"
#include "DOMWindow.h"
#include "DocLoader.h"
#include "DocumentFragment.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLDocument.h"
#include "HTMLTokenizer.h" // for parseHTMLDocumentFragment
#include "Page.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "Text.h"
#include "TextResourceDecoder.h"
#include "XMLTokenizer.h"
#include "XSLTExtensions.h"
#include "XSLTUnicodeSort.h"
#include "loader.h"
#include "markup.h"
#include <libxslt/imports.h>
#include <libxslt/variables.h>
#include <libxslt/xsltutils.h>
#include <wtf/Assertions.h>
#include <wtf/Platform.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
#include "SoftLinking.h"

SOFT_LINK_LIBRARY(libxslt);
SOFT_LINK(libxslt, xsltFreeStylesheet, void, (xsltStylesheetPtr sheet), (sheet))
SOFT_LINK(libxslt, xsltFreeTransformContext, void, (xsltTransformContextPtr ctxt), (ctxt))
SOFT_LINK(libxslt, xsltNewTransformContext, xsltTransformContextPtr, (xsltStylesheetPtr style, xmlDocPtr doc), (style, doc))
SOFT_LINK(libxslt, xsltApplyStylesheetUser, xmlDocPtr, (xsltStylesheetPtr style, xmlDocPtr doc, const char** params, const char* output, FILE* profile, xsltTransformContextPtr userCtxt), (style, doc, params, output, profile, userCtxt))
SOFT_LINK(libxslt, xsltQuoteUserParams, int, (xsltTransformContextPtr ctxt, const char** params), (ctxt, params))
SOFT_LINK(libxslt, xsltSetCtxtSortFunc, void, (xsltTransformContextPtr ctxt, xsltSortFunc handler), (ctxt, handler))
SOFT_LINK(libxslt, xsltSetLoaderFunc, void, (xsltDocLoaderFunc f), (f))
SOFT_LINK(libxslt, xsltSaveResultTo, int, (xmlOutputBufferPtr buf, xmlDocPtr result, xsltStylesheetPtr style), (buf, result, style))
SOFT_LINK(libxslt, xsltNextImport, xsltStylesheetPtr, (xsltStylesheetPtr style), (style))
#endif

namespace WebCore {

void XSLTProcessor::genericErrorFunc(void*, const char*, ...)
{
    // It would be nice to do something with this error message.
}
    
void XSLTProcessor::parseErrorFunc(void* userData, xmlError* error)
{
    Console* console = static_cast<Console*>(userData);
    if (!console)
        return;

    MessageLevel level;
    switch (error->level) {
        case XML_ERR_NONE:
            level = TipMessageLevel;
            break;
        case XML_ERR_WARNING:
            level = WarningMessageLevel;
            break;
        case XML_ERR_ERROR:
        case XML_ERR_FATAL:
        default:
            level = ErrorMessageLevel;
            break;
    }

    console->addMessage(XMLMessageSource, level, error->message, error->line, error->file);
}

// FIXME: There seems to be no way to control the ctxt pointer for loading here, thus we have globals.
static XSLTProcessor* globalProcessor = 0;
static DocLoader* globalDocLoader = 0;
static xmlDocPtr docLoaderFunc(const xmlChar* uri,
                                    xmlDictPtr,
                                    int options,
                                    void* ctxt,
                                    xsltLoadType type)
{
    if (!globalProcessor)
        return 0;
    
    switch (type) {
        case XSLT_LOAD_DOCUMENT: {
            xsltTransformContextPtr context = (xsltTransformContextPtr)ctxt;
            xmlChar* base = xmlNodeGetBase(context->document->doc, context->node);
            KURL url(KURL(reinterpret_cast<const char*>(base)), reinterpret_cast<const char*>(uri));
            xmlFree(base);
            ResourceError error;
            ResourceResponse response;

            Vector<char> data;

            bool requestAllowed = globalDocLoader->frame() && globalDocLoader->doc()->securityOrigin()->canRequest(url);
            if (requestAllowed) {
                globalDocLoader->frame()->loader()->loadResourceSynchronously(url, AllowStoredCredentials, error, response, data);
                requestAllowed = globalDocLoader->doc()->securityOrigin()->canRequest(response.url());
            }
            if (!requestAllowed) {
                data.clear();
                globalDocLoader->printAccessDeniedMessage(url);
            }

            Console* console = 0;
            if (Frame* frame = globalProcessor->xslStylesheet()->ownerDocument()->frame())
                console = frame->domWindow()->console();
            xmlSetStructuredErrorFunc(console, XSLTProcessor::parseErrorFunc);
            xmlSetGenericErrorFunc(console, XSLTProcessor::genericErrorFunc);
            
            // We don't specify an encoding here. Neither Gecko nor WinIE respects
            // the encoding specified in the HTTP headers.
            xmlDocPtr doc = xmlReadMemory(data.data(), data.size(), (const char*)uri, 0, options);

            xmlSetStructuredErrorFunc(0, 0);
            xmlSetGenericErrorFunc(0, 0);

            return doc;
        }
        case XSLT_LOAD_STYLESHEET:
            return globalProcessor->xslStylesheet()->locateStylesheetSubResource(((xsltStylesheetPtr)ctxt)->doc, uri);
        default:
            break;
    }
    
    return 0;
}

static inline void setXSLTLoadCallBack(xsltDocLoaderFunc func, XSLTProcessor* processor, DocLoader* loader)
{
    xsltSetLoaderFunc(func);
    globalProcessor = processor;
    globalDocLoader = loader;
}

static int writeToVector(void* context, const char* buffer, int len)
{
    Vector<UChar>& resultOutput = *static_cast<Vector<UChar>*>(context);
    String decodedChunk = String::fromUTF8(buffer, len);
    resultOutput.append(decodedChunk.characters(), decodedChunk.length());
    return len;
}

static bool saveResultToString(xmlDocPtr resultDoc, xsltStylesheetPtr sheet, String& resultString)
{
    xmlOutputBufferPtr outputBuf = xmlAllocOutputBuffer(0);
    if (!outputBuf)
        return false;

    Vector<UChar> resultVector;
    outputBuf->context = &resultVector;
    outputBuf->writecallback = writeToVector;
    
    int retval = xsltSaveResultTo(outputBuf, resultDoc, sheet);
    xmlOutputBufferClose(outputBuf);
    if (retval < 0)
        return false;

    // Workaround for <http://bugzilla.gnome.org/show_bug.cgi?id=495668>: libxslt appends an extra line feed to the result.
    if (resultVector.size() > 0 && resultVector[resultVector.size() - 1] == '\n')
        resultVector.removeLast();

    resultString = String::adopt(resultVector);

    return true;
}

static inline void transformTextStringToXHTMLDocumentString(String& text)
{
    // Modify the output so that it is a well-formed XHTML document with a <pre> tag enclosing the text.
    text.replace('&', "&amp;");
    text.replace('<', "&lt;");
    text = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n"
        "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
        "<head><title/></head>\n"
        "<body>\n"
        "<pre>" + text + "</pre>\n"
        "</body>\n"
        "</html>\n";
}

static const char** xsltParamArrayFromParameterMap(XSLTProcessor::ParameterMap& parameters)
{
    if (parameters.isEmpty())
        return 0;

    const char** parameterArray = (const char**)fastMalloc(((parameters.size() * 2) + 1) * sizeof(char*));

    XSLTProcessor::ParameterMap::iterator end = parameters.end();
    unsigned index = 0;
    for (XSLTProcessor::ParameterMap::iterator it = parameters.begin(); it != end; ++it) {
        parameterArray[index++] = strdup(it->first.utf8().data());
        parameterArray[index++] = strdup(it->second.utf8().data());
    }
    parameterArray[index] = 0;

    return parameterArray;
}

static void freeXsltParamArray(const char** params)
{
    const char** temp = params;
    if (!params)
        return;
    
    while (*temp) {
        free((void*)*(temp++)); // strdup returns malloc'd blocks, so we have to use free() here
        free((void*)*(temp++));
    }
    fastFree(params);
}


PassRefPtr<Document> XSLTProcessor::createDocumentFromSource(const String& sourceString,
    const String& sourceEncoding, const String& sourceMIMEType, Node* sourceNode, Frame* frame)
{
    RefPtr<Document> ownerDocument = sourceNode->document();
    bool sourceIsDocument = (sourceNode == ownerDocument.get());
    String documentSource = sourceString;

    RefPtr<Document> result;
    if (sourceMIMEType == "text/plain") {
        result = ownerDocument->implementation()->createDocument(frame);
        transformTextStringToXHTMLDocumentString(documentSource);
    } else
        result = ownerDocument->implementation()->createDocument(sourceMIMEType, frame, false);
    
    // Before parsing, we need to save & detach the old document and get the new document
    // in place. We have to do this only if we're rendering the result document.
    if (frame) {
        if (FrameView* view = frame->view())
            view->clear();
        result->setTransformSourceDocument(frame->document());
        frame->setDocument(result);
    }
    
    if (sourceIsDocument)
        result->setURL(ownerDocument->url());
    result->open();
    
    RefPtr<TextResourceDecoder> decoder = TextResourceDecoder::create(sourceMIMEType);
    decoder->setEncoding(sourceEncoding.isEmpty() ? UTF8Encoding() : TextEncoding(sourceEncoding), TextResourceDecoder::EncodingFromXMLHeader);
    result->setDecoder(decoder.release());
    
    result->write(documentSource);
    result->finishParsing();
    result->close();

    return result.release();
}

static inline RefPtr<DocumentFragment> createFragmentFromSource(const String& sourceString, const String& sourceMIMEType, Document* outputDoc)
{
    RefPtr<DocumentFragment> fragment = new DocumentFragment(outputDoc);
    
    if (sourceMIMEType == "text/html")
        parseHTMLDocumentFragment(sourceString, fragment.get());
    else if (sourceMIMEType == "text/plain")
        fragment->addChild(new Text(outputDoc, sourceString));
    else {
        bool successfulParse = parseXMLDocumentFragment(sourceString, fragment.get(), outputDoc->documentElement());
        if (!successfulParse)
            return 0;
    }
    
    // FIXME: Do we need to mess with URLs here?
        
    return fragment;
}

static xsltStylesheetPtr xsltStylesheetPointer(RefPtr<XSLStyleSheet>& cachedStylesheet, Node* stylesheetRootNode)
{
    if (!cachedStylesheet && stylesheetRootNode) {
        cachedStylesheet = XSLStyleSheet::create(stylesheetRootNode->parent() ? stylesheetRootNode->parent() : stylesheetRootNode,
            stylesheetRootNode->document()->url().string());
        cachedStylesheet->parseString(createMarkup(stylesheetRootNode));
    }
    
    if (!cachedStylesheet || !cachedStylesheet->document())
        return 0;
    
    return cachedStylesheet->compileStyleSheet();
}

static inline xmlDocPtr xmlDocPtrFromNode(Node* sourceNode, bool& shouldDelete)
{
    RefPtr<Document> ownerDocument = sourceNode->document();
    bool sourceIsDocument = (sourceNode == ownerDocument.get());
    
    xmlDocPtr sourceDoc = 0;
    if (sourceIsDocument)
        sourceDoc = (xmlDocPtr)ownerDocument->transformSource();
    if (!sourceDoc) {
        sourceDoc = (xmlDocPtr)xmlDocPtrForString(ownerDocument->docLoader(), createMarkup(sourceNode),
            sourceIsDocument ? ownerDocument->url().string() : String());
        shouldDelete = (sourceDoc != 0);
    }
    return sourceDoc;
}

static inline String resultMIMEType(xmlDocPtr resultDoc, xsltStylesheetPtr sheet)
{
    // There are three types of output we need to be able to deal with:
    // HTML (create an HTML document), XML (create an XML document),
    // and text (wrap in a <pre> and create an XML document).

    const xmlChar* resultType = 0;
    XSLT_GET_IMPORT_PTR(resultType, sheet, method);
    if (resultType == 0 && resultDoc->type == XML_HTML_DOCUMENT_NODE)
        resultType = (const xmlChar*)"html";
    
    if (xmlStrEqual(resultType, (const xmlChar*)"html"))
        return "text/html";
    else if (xmlStrEqual(resultType, (const xmlChar*)"text"))
        return "text/plain";
        
    return "application/xml";
}

bool XSLTProcessor::transformToString(Node* sourceNode, String& mimeType, String& resultString, String& resultEncoding)
{
    RefPtr<Document> ownerDocument = sourceNode->document();
    
    setXSLTLoadCallBack(docLoaderFunc, this, ownerDocument->docLoader());
    xsltStylesheetPtr sheet = xsltStylesheetPointer(m_stylesheet, m_stylesheetRootNode.get());
    if (!sheet) {
        setXSLTLoadCallBack(0, 0, 0);
        return false;
    }
    m_stylesheet->clearDocuments();

    xmlChar* origMethod = sheet->method;
    if (!origMethod && mimeType == "text/html")
        sheet->method = (xmlChar*)"html";

    bool success = false;
    bool shouldFreeSourceDoc = false;
    if (xmlDocPtr sourceDoc = xmlDocPtrFromNode(sourceNode, shouldFreeSourceDoc)) {
        // The XML declaration would prevent parsing the result as a fragment, and it's not needed even for documents, 
        // as the result of this function is always immediately parsed.
        sheet->omitXmlDeclaration = true;

        xsltTransformContextPtr transformContext = xsltNewTransformContext(sheet, sourceDoc);
        registerXSLTExtensions(transformContext);

        // <http://bugs.webkit.org/show_bug.cgi?id=16077>: XSLT processor <xsl:sort> algorithm only compares by code point
        xsltSetCtxtSortFunc(transformContext, xsltUnicodeSortFunction);

        // This is a workaround for a bug in libxslt. 
        // The bug has been fixed in version 1.1.13, so once we ship that this can be removed.
        if (transformContext->globalVars == NULL)
           transformContext->globalVars = xmlHashCreate(20);

        const char** params = xsltParamArrayFromParameterMap(m_parameters);
        xsltQuoteUserParams(transformContext, params);
        xmlDocPtr resultDoc = xsltApplyStylesheetUser(sheet, sourceDoc, 0, 0, 0, transformContext);
        
        xsltFreeTransformContext(transformContext);        
        freeXsltParamArray(params);
        
        if (shouldFreeSourceDoc)
            xmlFreeDoc(sourceDoc);
        
        if (success = saveResultToString(resultDoc, sheet, resultString)) {
            mimeType = resultMIMEType(resultDoc, sheet);
            resultEncoding = (char*)resultDoc->encoding;
        }
        xmlFreeDoc(resultDoc);
    }
    
    sheet->method = origMethod;
    setXSLTLoadCallBack(0, 0, 0);
    xsltFreeStylesheet(sheet);
    m_stylesheet = 0;

    return success;
}

PassRefPtr<Document> XSLTProcessor::transformToDocument(Node* sourceNode)
{
    String resultMIMEType;
    String resultString;
    String resultEncoding;
    if (!transformToString(sourceNode, resultMIMEType, resultString, resultEncoding))
        return 0;
    return createDocumentFromSource(resultString, resultEncoding, resultMIMEType, sourceNode, 0);
}

PassRefPtr<DocumentFragment> XSLTProcessor::transformToFragment(Node* sourceNode, Document* outputDoc)
{
    String resultMIMEType;
    String resultString;
    String resultEncoding;

    // If the output document is HTML, default to HTML method.
    if (outputDoc->isHTMLDocument())
        resultMIMEType = "text/html";
    
    if (!transformToString(sourceNode, resultMIMEType, resultString, resultEncoding))
        return 0;
    return createFragmentFromSource(resultString, resultMIMEType, outputDoc);
}

void XSLTProcessor::setParameter(const String& /*namespaceURI*/, const String& localName, const String& value)
{
    // FIXME: namespace support?
    // should make a QualifiedName here but we'd have to expose the impl
    m_parameters.set(localName, value);
}

String XSLTProcessor::getParameter(const String& /*namespaceURI*/, const String& localName) const
{
    // FIXME: namespace support?
    // should make a QualifiedName here but we'd have to expose the impl
    return m_parameters.get(localName);
}

void XSLTProcessor::removeParameter(const String& /*namespaceURI*/, const String& localName)
{
    // FIXME: namespace support?
    m_parameters.remove(localName);
}

} // namespace WebCore

#endif // ENABLE(XSLT)
