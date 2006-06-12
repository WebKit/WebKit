/**
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#ifdef KHTML_XSLT

#include "XSLTProcessor.h"

#include "Cache.h"
#include "DOMImplementation.h"
#include "Decoder.h"
#include "DocLoader.h"
#include "DocumentFragment.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLTokenizer.h"
#include "KWQLoader.h"
#include "Text.h"
#include "TransferJob.h"
#include "loader.h"
#include "markup.h"
#include <wtf/Vector.h>
#include <libxslt/imports.h>
#include <libxslt/xsltutils.h>

namespace WebCore {

static void parseErrorFunc(void *ctxt, const char *msg, ...)
{
    // FIXME: It would be nice to display error messages somewhere.
#if !WIN32
    // FIXME: No vasprintf support.
#if !ERROR_DISABLED
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
static WebCore::DocLoader *globalDocLoader = 0;
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
            KURL finalURL;
            TransferJob* job = new TransferJob(0, "GET", url);
            DeprecatedString headers;
            xmlGenericErrorFunc oldErrorFunc = xmlGenericError;
            void *oldErrorContext = xmlGenericErrorContext;
            
            Vector<char> data = KWQServeSynchronousRequest(Cache::loader(), globalDocLoader, job, finalURL, headers);
        
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

static inline void setXSLTLoadCallBack(xsltDocLoaderFunc func, XSLTProcessor *processor, WebCore::DocLoader *loader)
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

static inline void transformTextStringToXHTMLDocumentString(DeprecatedString &text)
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
        parameterArray[index++] = strdup(String(it->first.get()).deprecatedString().utf8().data());
        parameterArray[index++] = strdup(String(it->second.get()).deprecatedString().utf8().data());
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


RefPtr<Document> XSLTProcessor::createDocumentFromSource(const DeprecatedString &sourceString, const DeprecatedString &sourceEncoding, const DeprecatedString &sourceMIMEType, Node *sourceNode, FrameView *view)
{
    RefPtr<Document> ownerDocument = sourceNode->document();
    bool sourceIsDocument = (sourceNode == ownerDocument.get());
    DeprecatedString documentSource = sourceString;

    RefPtr<Document> result;
    if (sourceMIMEType == "text/html")
        result = ownerDocument->implementation()->createHTMLDocument(view);
    else {
        result = ownerDocument->implementation()->createDocument(view);
        if (sourceMIMEType == "text/plain")
            transformTextStringToXHTMLDocumentString(documentSource);
    }
    
    // Before parsing, we need to save & detach the old document and get the new document
    // in place. We have to do this only if we're rendering the result document.
    if (view) {
        view->clear();
        result->setTransformSourceDocument(view->frame()->document());
        view->frame()->setDocument(result.get());
    }
    
    result->open();
    if (sourceIsDocument) {
        result->setURL(ownerDocument->URL());
        result->setBaseURL(ownerDocument->baseURL());
    }
    result->determineParseMode(documentSource); // Make sure we parse in the correct mode.
    
    RefPtr<Decoder> decoder = new Decoder;
    decoder->setEncodingName(sourceEncoding.isEmpty() ? "UTF-8" : sourceEncoding.latin1(), Decoder::EncodingFromXMLHeader);
    result->setDecoder(decoder.get());
    
    result->write(documentSource);
    result->finishParsing();
    if (view)
        view->frame()->checkCompleted();
    else
        result->close(); // FIXME: Even viewless docs can load subresources. onload will fire too early.
                         // This is probably a bug in XMLHttpRequestObjects as well.
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
        cachedStylesheet = new XSLStyleSheet(stylesheetRootNode->parent() ? stylesheetRootNode->parent() : stylesheetRootNode);
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
        sourceDoc = (xmlDocPtr)xmlDocPtrForString(createMarkup(sourceNode), sourceIsDocument ? ownerDocument->URL() : DeprecatedString());
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
    RefPtr<XSLStyleSheet> cachedStylesheet = m_stylesheet;
    
    setXSLTLoadCallBack(docLoaderFunc, this, ownerDocument->docLoader());
    xsltStylesheetPtr sheet = xsltStylesheetPointer(cachedStylesheet, m_stylesheetRootNode.get());
    if (!sheet) {
        setXSLTLoadCallBack(0, 0, 0);
        return false;
    }
    cachedStylesheet->clearDocuments();
    
    bool success = false;
    bool shouldFreeSourceDoc = false;
    if (xmlDocPtr sourceDoc = xmlDocPtrFromNode(sourceNode, shouldFreeSourceDoc)) {
        const char **params = xsltParamArrayFromParameterMap(m_parameters);
        xmlDocPtr resultDoc = xsltApplyStylesheet(sheet, sourceDoc, params);
        freeXsltParamArray(params);
        if (shouldFreeSourceDoc)
            xmlFreeDoc(sourceDoc);
        
        if (success = saveResultToString(resultDoc, sheet, resultString)) {
            mimeType = resultMIMEType(resultDoc, sheet);
            resultEncoding = (char *)resultDoc->encoding;
        }
        xmlFreeDoc(resultDoc);
    }
    
    setXSLTLoadCallBack(0, 0, 0);
    xsltFreeStylesheet(sheet);

    return success;
}

RefPtr<Document> XSLTProcessor::transformToDocument(Node *sourceNode)
{
    DeprecatedString resultMIMEType;
    DeprecatedString resultString;
    DeprecatedString resultEncoding;
    if (!transformToString(sourceNode, resultMIMEType, resultString, resultEncoding))
        return 0;
    return createDocumentFromSource(resultString, resultEncoding, resultMIMEType, sourceNode);
}

RefPtr<DocumentFragment> XSLTProcessor::transformToFragment(Node *sourceNode, Document *outputDoc)
{
    DeprecatedString resultMIMEType;
    DeprecatedString resultString;
    DeprecatedString resultEncoding;
    if (!transformToString(sourceNode, resultMIMEType, resultString, resultEncoding))
        return 0;
    return createFragmentFromSource(resultString, resultMIMEType, sourceNode, outputDoc);
}

void XSLTProcessor::setParameter(StringImpl *namespaceURI, StringImpl *localName, StringImpl *value)
{
    // FIXME: namespace support?
    // should make a QualifiedName here but we'd have to expose the impl
    m_parameters.set(localName, value);
}

RefPtr<StringImpl> XSLTProcessor::getParameter(StringImpl *namespaceURI, StringImpl *localName) const
{
    // FIXME: namespace support?
    // should make a QualifiedName here but we'd have to expose the impl
    return m_parameters.get(localName);
}

void XSLTProcessor::removeParameter(StringImpl *namespaceURI, StringImpl *localName)
{
    // FIXME: namespace support?
    m_parameters.remove(localName);
}

}

#endif
