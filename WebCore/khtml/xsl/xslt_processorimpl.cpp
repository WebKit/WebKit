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

#include "xslt_processorimpl.h"
#include "xsl_stylesheetimpl.h"
#include "xml_tokenizer.h"
#include "TextImpl.h"
#include "htmltokenizer.h"
#include "html_documentimpl.h"
#include "DOMImplementationImpl.h"
#include "loader.h"
#include "Cache.h"
#include "DocLoader.h"
#include "markup.h"
#include "FrameView.h"
#include "Frame.h"
#include "KWQLoader.h"

#include <kio/job.h>

#include <libxslt/xsltutils.h>
#include <libxslt/documents.h>
#include <libxslt/imports.h>

#include <kxmlcore/Assertions.h>

using namespace khtml;
using namespace DOM;

namespace DOM {

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
    ERROR("%s", errorMessage);
    if (errorMessage)
        free(errorMessage);
    va_end(args);
#endif
#endif
}

// FIXME: There seems to be no way to control the ctxt pointer for loading here, thus we have globals.
static XSLTProcessorImpl *globalProcessor = 0;
static khtml::DocLoader *globalDocLoader = 0;
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
            KIO::TransferJob *job = KIO::get(url, true, false);
            QString headers;
            xmlGenericErrorFunc oldErrorFunc = xmlGenericError;
            void *oldErrorContext = xmlGenericErrorContext;
            
            ByteArray data = KWQServeSynchronousRequest(Cache::loader(), globalDocLoader, job, finalURL, headers);
        
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

static inline void setXSLTLoadCallBack(xsltDocLoaderFunc func, XSLTProcessorImpl *processor, khtml::DocLoader *loader)
{
    xsltSetLoaderFunc(func);
    globalProcessor = processor;
    globalDocLoader = loader;
}

static int writeToQString(void *context, const char *buffer, int len)
{
    QString &resultOutput = *static_cast<QString *>(context);
    resultOutput += QString::fromUtf8(buffer, len);
    return len;
}

static bool saveResultToString(xmlDocPtr resultDoc, xsltStylesheetPtr sheet, QString &resultString)
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

static inline void transformTextStringToXHTMLDocumentString(QString &text)
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

static const char **xsltParamArrayFromParameterMap(XSLTProcessorImpl::ParameterMap& parameters)
{
    if (parameters.isEmpty())
        return 0;

    const char **parameterArray = (const char **)fastMalloc(((parameters.size() * 2) + 1) * sizeof(char *));

    XSLTProcessorImpl::ParameterMap::iterator end = parameters.end();
    unsigned index = 0;
    for (XSLTProcessorImpl::ParameterMap::iterator it = parameters.begin(); it != end; ++it) {
        parameterArray[index++] = strdup(DOMString(it->first.get()).qstring().utf8().data());
        parameterArray[index++] = strdup(DOMString(it->second.get()).qstring().utf8().data());
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


RefPtr<DocumentImpl> XSLTProcessorImpl::createDocumentFromSource(const QString &sourceString, const QString &sourceEncoding, const QString &sourceMIMEType, NodeImpl *sourceNode, FrameView *view)
{
    RefPtr<DocumentImpl> ownerDocument = sourceNode->getDocument();
    bool sourceIsDocument = (sourceNode == ownerDocument.get());
    QString documentSource = sourceString;

    RefPtr<DocumentImpl> result;
    if (sourceMIMEType == "text/html")
        result = ownerDocument->implementation()->createHTMLDocument(view);
    else {
        result = ownerDocument->implementation()->createDocument(view);
        if (sourceMIMEType == "text/plain")
            transformTextStringToXHTMLDocumentString(documentSource);
    }
    
    result->docLoader()->setShowAnimations(ownerDocument->docLoader()->showAnimations());
    
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
    decoder->setEncoding(sourceEncoding.isEmpty() ? "UTF-8" : sourceEncoding.latin1(), Decoder::EncodingFromXMLHeader);
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

static inline RefPtr<DocumentFragmentImpl> createFragmentFromSource(QString sourceString, QString sourceMIMEType, NodeImpl *sourceNode, DocumentImpl *outputDoc)
{
    RefPtr<DocumentFragmentImpl> fragment = new DocumentFragmentImpl(outputDoc);
    
    if (sourceMIMEType == "text/html")
        parseHTMLDocumentFragment(sourceString, fragment.get());
    else if (sourceMIMEType == "text/plain")
        fragment->addChild(new TextImpl(outputDoc, sourceString));
    else {
        bool successfulParse = parseXMLDocumentFragment(sourceString, fragment.get(), outputDoc->documentElement());
        if (!successfulParse)
            return 0;
    }
    
    // FIXME: Do we need to mess with URLs here?
        
    return fragment;
}

static xsltStylesheetPtr xsltStylesheetPointer(RefPtr<XSLStyleSheetImpl> &cachedStylesheet, NodeImpl *stylesheetRootNode)
{
    if (!cachedStylesheet && stylesheetRootNode) {
        cachedStylesheet = new XSLStyleSheetImpl(stylesheetRootNode->parent() ? stylesheetRootNode->parent() : stylesheetRootNode);
        cachedStylesheet->parseString(createMarkup(stylesheetRootNode));
    }
    
    if (!cachedStylesheet || !cachedStylesheet->document())
        return 0;
    
    return cachedStylesheet->compileStyleSheet();
}

static inline xmlDocPtr xmlDocPtrFromNode(NodeImpl *sourceNode, bool &shouldDelete)
{
    RefPtr<DocumentImpl> ownerDocument = sourceNode->getDocument();
    bool sourceIsDocument = (sourceNode == ownerDocument.get());
    
    xmlDocPtr sourceDoc = 0;
    if (sourceIsDocument)
        sourceDoc = (xmlDocPtr)ownerDocument->transformSource();
    if (!sourceDoc) {
        sourceDoc = (xmlDocPtr)xmlDocPtrForString(createMarkup(sourceNode), sourceIsDocument ? ownerDocument->URL() : QString());
        shouldDelete = (sourceDoc != 0);
    }
    return sourceDoc;
}

static inline QString resultMIMEType(xmlDocPtr resultDoc, xsltStylesheetPtr sheet)
{
    // There are three types of output we need to be able to deal with:
    // HTML (create an HTML document), XML (create an XML document),
    // and text (wrap in a <pre> and create an XML document).

    const xmlChar *resultType = 0;
    XSLT_GET_IMPORT_PTR(resultType, sheet, method);
    if (resultType == 0 && resultDoc->type == XML_HTML_DOCUMENT_NODE)
        resultType = (const xmlChar *)"html";
    
    if (xmlStrEqual(resultType, (const xmlChar *)"html"))
        return QString("text/html");
    else if (xmlStrEqual(resultType, (const xmlChar *)"text"))
        return QString("text/plain");
        
    return QString("application/xml");
}

bool XSLTProcessorImpl::transformToString(NodeImpl *sourceNode, QString &mimeType, QString &resultString, QString &resultEncoding)
{
    RefPtr<DocumentImpl> ownerDocument = sourceNode->getDocument();
    RefPtr<XSLStyleSheetImpl> cachedStylesheet = m_stylesheet;
    
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

RefPtr<DocumentImpl> XSLTProcessorImpl::transformToDocument(NodeImpl *sourceNode)
{
    QString resultMIMEType;
    QString resultString;
    QString resultEncoding;
    if (!transformToString(sourceNode, resultMIMEType, resultString, resultEncoding))
        return 0;
    return createDocumentFromSource(resultString, resultEncoding, resultMIMEType, sourceNode);
}

RefPtr<DocumentFragmentImpl> XSLTProcessorImpl::transformToFragment(NodeImpl *sourceNode, DocumentImpl *outputDoc)
{
    QString resultMIMEType;
    QString resultString;
    QString resultEncoding;
    if (!transformToString(sourceNode, resultMIMEType, resultString, resultEncoding))
        return 0;
    return createFragmentFromSource(resultString, resultMIMEType, sourceNode, outputDoc);
}

void XSLTProcessorImpl::setParameter(DOMStringImpl *namespaceURI, DOMStringImpl *localName, DOMStringImpl *value)
{
    // FIXME: namespace support?
    // should make a QualifiedName here but we'd have to expose the impl
    m_parameters.set(localName, value);
}

RefPtr<DOMStringImpl> XSLTProcessorImpl::getParameter(DOMStringImpl *namespaceURI, DOMStringImpl *localName) const
{
    // FIXME: namespace support?
    // should make a QualifiedName here but we'd have to expose the impl
    return m_parameters.get(localName);
}

void XSLTProcessorImpl::removeParameter(DOMStringImpl *namespaceURI, DOMStringImpl *localName)
{
    // FIXME: namespace support?
    m_parameters.remove(localName);
}

}

#endif
