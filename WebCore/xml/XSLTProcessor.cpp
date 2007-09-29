/**
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
#include "Cache.h"
#include "DOMImplementation.h"
#include "DocLoader.h"
#include "DocumentFragment.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLDocument.h"
#include "HTMLTokenizer.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "Text.h"
#include "TextResourceDecoder.h"
#include "XMLTokenizer.h"
#include "XSLTExtensions.h"
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
#endif

#if PLATFORM(MAC)
SOFT_LINK_LIBRARY(libxslt);
SOFT_LINK(libxslt, xsltFreeStylesheet, void, (xsltStylesheetPtr sheet), (sheet))
SOFT_LINK(libxslt, xsltFreeTransformContext, void, (xsltTransformContextPtr ctxt), (ctxt))
SOFT_LINK(libxslt, xsltNewTransformContext, xsltTransformContextPtr, (xsltStylesheetPtr style, xmlDocPtr doc), (style, doc))
SOFT_LINK(libxslt, xsltApplyStylesheetUser, xmlDocPtr, (xsltStylesheetPtr style, xmlDocPtr doc, const char **params, const char *output, FILE * profile, xsltTransformContextPtr userCtxt), (style, doc, params, output, profile, userCtxt))
SOFT_LINK(libxslt, xsltQuoteUserParams, int, (xsltTransformContextPtr ctxt, const char **params), (ctxt, params))
SOFT_LINK(libxslt, xsltSetLoaderFunc, void, (xsltDocLoaderFunc f), (f))
SOFT_LINK(libxslt, xsltSaveResultTo, int, (xmlOutputBufferPtr buf, xmlDocPtr result, xsltStylesheetPtr style), (buf, result, style))
SOFT_LINK(libxslt, xsltNextImport, xsltStylesheetPtr, (xsltStylesheetPtr style), (style))
#endif

namespace WebCore {

static void parseErrorFunc(void *ctxt, const char *msg, ...)
{
    // FIXME: It would be nice to display error messages somewhere.
#if !PLATFORM(WIN_OS)
    // FIXME: No vasprintf support.
#ifndef ERROR_DISABLED
    char *errorMessage = 0;
    va_list args;
    va_start(args, msg);
    vasprintf(&errorMessage, msg, args);
    LOG_ERROR("%s", errorMessage);
    if (errorMessage)
        free(errorMessage);
    va_end(args);
#endif
#endif
}

// FIXME: There seems to be no way to control the ctxt pointer for loading here, thus we have globals.
static XSLTProcessor *globalProcessor = 0;
static DocLoader *globalDocLoader = 0;
static xmlDocPtr docLoaderFunc(const xmlChar *uri,
                                    xmlDictPtr dict,
                                    int options,
                                    void* ctxt,
                                    xsltLoadType type)
{
    if (!globalProcessor)
        return 0;
    
    switch (type) {
        case XSLT_LOAD_DOCUMENT: {
            xsltTransformContextPtr context = (xsltTransformContextPtr)ctxt;
            xmlChar *base = xmlNodeGetBase(context->document->doc, context->node);
            KURL url((const char*)base, (const char*)uri);
            xmlFree(base);
            ResourceError error;
            ResourceResponse response;
            xmlGenericErrorFunc oldErrorFunc = xmlGenericError;
            void *oldErrorContext = xmlGenericErrorContext;

            Vector<char> data;
                
            if (globalDocLoader->frame()) 
                globalDocLoader->frame()->loader()->loadResourceSynchronously(url, error, response, data);

            xmlSetGenericErrorFunc(0, parseErrorFunc);
            // We don't specify an encoding here. Neither Gecko nor WinIE respects
            // the encoding specified in the HTTP headers.
            xmlDocPtr doc = xmlReadMemory(data.data(), data.size(), (const char*)uri, 0, options);
            xmlSetGenericErrorFunc(oldErrorContext, oldErrorFunc);
            return doc;
        }
        case XSLT_LOAD_STYLESHEET:
            return globalProcessor->xslStylesheet()->locateStylesheetSubResource(((xsltStylesheetPtr)ctxt)->doc, uri);
        default:
            break;
    }
    
    return 0;
}

static inline void setXSLTLoadCallBack(xsltDocLoaderFunc func, XSLTProcessor *processor, DocLoader *loader)
{
    xsltSetLoaderFunc(func);
    globalProcessor = processor;
    globalDocLoader = loader;
}

static int writeToQString(void *context, const char *buffer, int len)
{
    DeprecatedString &resultOutput = *static_cast<DeprecatedString *>(context);
    resultOutput += DeprecatedString::fromUtf8(buffer, len);
    return len;
}

static bool saveResultToString(xmlDocPtr resultDoc, xsltStylesheetPtr sheet, DeprecatedString &resultString)
{
    xmlOutputBufferPtr outputBuf = xmlAllocOutputBuffer(0);
    if (!outputBuf)
        return false;
    outputBuf->context = &resultString;
    outputBuf->writecallback = writeToQString;
    
    int retval = xsltSaveResultTo(outputBuf, resultDoc, sheet);
    xmlOutputBufferClose(outputBuf);
    
    return (retval >= 0);
}

static inline void transformTextStringToXHTMLDocumentString(String &text)
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

static const char **xsltParamArrayFromParameterMap(XSLTProcessor::ParameterMap& parameters)
{
    if (parameters.isEmpty())
        return 0;

    const char **parameterArray = (const char **)fastMalloc(((parameters.size() * 2) + 1) * sizeof(char *));

    XSLTProcessor::ParameterMap::iterator end = parameters.end();
    unsigned index = 0;
    for (XSLTProcessor::ParameterMap::iterator it = parameters.begin(); it != end; ++it) {
        parameterArray[index++] = strdup(it->first.utf8().data());
        parameterArray[index++] = strdup(it->second.utf8().data());
    }
    parameterArray[index] = 0;

    return parameterArray;
}

static void freeXsltParamArray(const char **params)
{
    const char **temp = params;
    if (!params)
        return;
    
    while (*temp) {
        free((void *)*(temp++)); // strdup returns malloc'd blocks, so we have to use free() here
        free((void *)*(temp++));
    }
    fastFree(params);
}


RefPtr<Document> XSLTProcessor::createDocumentFromSource(const DeprecatedString& sourceString,
    const DeprecatedString& sourceEncoding, const DeprecatedString& sourceMIMEType, Node* sourceNode, Frame* frame)
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
    
    result->open();
    if (sourceIsDocument) {
        result->setURL(ownerDocument->URL());
        result->setBaseURL(ownerDocument->baseURL());
    }
    result->determineParseMode(documentSource); // Make sure we parse in the correct mode.
    
    RefPtr<TextResourceDecoder> decoder = new TextResourceDecoder(sourceMIMEType);
    decoder->setEncoding(sourceEncoding.isEmpty() ? UTF8Encoding() : TextEncoding(sourceEncoding), TextResourceDecoder::EncodingFromXMLHeader);
    result->setDecoder(decoder.get());
    
    result->write(documentSource);
    result->finishParsing();
    result->close();

    return result;
}

static inline RefPtr<DocumentFragment> createFragmentFromSource(DeprecatedString sourceString, DeprecatedString sourceMIMEType, Node *sourceNode, Document *outputDoc)
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

static xsltStylesheetPtr xsltStylesheetPointer(RefPtr<XSLStyleSheet> &cachedStylesheet, Node *stylesheetRootNode)
{
    if (!cachedStylesheet && stylesheetRootNode) {
        cachedStylesheet = new XSLStyleSheet(stylesheetRootNode->parent() ? stylesheetRootNode->parent() : stylesheetRootNode, stylesheetRootNode->document()->URL());
        cachedStylesheet->parseString(createMarkup(stylesheetRootNode));
    }
    
    if (!cachedStylesheet || !cachedStylesheet->document())
        return 0;
    
    return cachedStylesheet->compileStyleSheet();
}

static inline xmlDocPtr xmlDocPtrFromNode(Node *sourceNode, bool &shouldDelete)
{
    RefPtr<Document> ownerDocument = sourceNode->document();
    bool sourceIsDocument = (sourceNode == ownerDocument.get());
    
    xmlDocPtr sourceDoc = 0;
    if (sourceIsDocument)
        sourceDoc = (xmlDocPtr)ownerDocument->transformSource();
    if (!sourceDoc) {
        sourceDoc = (xmlDocPtr)xmlDocPtrForString(ownerDocument->docLoader(), createMarkup(sourceNode), sourceIsDocument ? ownerDocument->URL() : DeprecatedString());
        shouldDelete = (sourceDoc != 0);
    }
    return sourceDoc;
}

static inline DeprecatedString resultMIMEType(xmlDocPtr resultDoc, xsltStylesheetPtr sheet)
{
    // There are three types of output we need to be able to deal with:
    // HTML (create an HTML document), XML (create an XML document),
    // and text (wrap in a <pre> and create an XML document).

    const xmlChar *resultType = 0;
    XSLT_GET_IMPORT_PTR(resultType, sheet, method);
    if (resultType == 0 && resultDoc->type == XML_HTML_DOCUMENT_NODE)
        resultType = (const xmlChar *)"html";
    
    if (xmlStrEqual(resultType, (const xmlChar *)"html"))
        return DeprecatedString("text/html");
    else if (xmlStrEqual(resultType, (const xmlChar *)"text"))
        return DeprecatedString("text/plain");
        
    return DeprecatedString("application/xml");
}

bool XSLTProcessor::transformToString(Node *sourceNode, DeprecatedString &mimeType, DeprecatedString &resultString, DeprecatedString &resultEncoding)
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

        // This is a workaround for a bug in libxslt. 
        // The bug has been fixed in version 1.1.13, so once we ship that this can be removed.
        if (transformContext->globalVars == NULL)
           transformContext->globalVars = xmlHashCreate(20);

        const char **params = xsltParamArrayFromParameterMap(m_parameters);
        xsltQuoteUserParams(transformContext, params);
        xmlDocPtr resultDoc = xsltApplyStylesheetUser(sheet, sourceDoc, 0, 0, 0, transformContext);
        
        xsltFreeTransformContext(transformContext);        
        freeXsltParamArray(params);
        
        if (shouldFreeSourceDoc)
            xmlFreeDoc(sourceDoc);
        
        if (success = saveResultToString(resultDoc, sheet, resultString)) {
            mimeType = resultMIMEType(resultDoc, sheet);
            resultEncoding = (char *)resultDoc->encoding;
        }
        xmlFreeDoc(resultDoc);
    }
    
    sheet->method = origMethod;
    setXSLTLoadCallBack(0, 0, 0);
    xsltFreeStylesheet(sheet);
    m_stylesheet = 0;

    return success;
}

RefPtr<Document> XSLTProcessor::transformToDocument(Node *sourceNode)
{
    DeprecatedString resultMIMEType;
    DeprecatedString resultString;
    DeprecatedString resultEncoding;
    if (!transformToString(sourceNode, resultMIMEType, resultString, resultEncoding))
        return 0;
    return createDocumentFromSource(resultString, resultEncoding, resultMIMEType, sourceNode, 0);
}

RefPtr<DocumentFragment> XSLTProcessor::transformToFragment(Node* sourceNode, Document* outputDoc)
{
    DeprecatedString resultMIMEType;
    DeprecatedString resultString;
    DeprecatedString resultEncoding;

    // If the output document is HTML, default to HTML method.
    if (outputDoc->isHTMLDocument())
        resultMIMEType = "text/html";
    
    if (!transformToString(sourceNode, resultMIMEType, resultString, resultEncoding))
        return 0;
    return createFragmentFromSource(resultString, resultMIMEType, sourceNode, outputDoc);
}

void XSLTProcessor::setParameter(const String& namespaceURI, const String& localName, const String& value)
{
    // FIXME: namespace support?
    // should make a QualifiedName here but we'd have to expose the impl
    m_parameters.set(localName, value);
}

String XSLTProcessor::getParameter(const String& namespaceURI, const String& localName) const
{
    // FIXME: namespace support?
    // should make a QualifiedName here but we'd have to expose the impl
    return m_parameters.get(localName);
}

void XSLTProcessor::removeParameter(const String& namespaceURI, const String& localName)
{
    // FIXME: namespace support?
    m_parameters.remove(localName);
}

} // namespace WebCore

#endif // ENABLE(XSLT)
