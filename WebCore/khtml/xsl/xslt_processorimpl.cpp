/**
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004 Apple Computer, Inc.
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

#ifdef KHTML_XSLT

#include "config.h"
#include "xslt_processorimpl.h"
#include "xsl_stylesheetimpl.h"
#include "xml_tokenizer.h"
#include "htmltokenizer.h"
#include "html_documentimpl.h"
#include "loader.h"
#include "markup.h"
#include "khtmlview.h"
#include "khtml_part.h"
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
}

// FIXME: There seems to be no way to control the ctxt pointer for loading here, thus we have globals.
static XSLTProcessorImpl *globalProcessor = 0;
static khtml::DocLoader *globalDocLoader = 0;
static xmlDocPtr stylesheetLoadFunc(const xmlChar *uri,
                                    xmlDictPtr dict,
                                    int options,
                                    void* ctxt,
                                    xsltLoadType type)
{
    if (!globalProcessor)
        return 0;
    
    switch (type) {
        case XSLT_LOAD_DOCUMENT: {
            KURL url = KURL((char *)uri);
            KURL finalURL;
            KIO::TransferJob *job = KIO::get(url, true, false);
            QByteArray data;
            QString headers;
            xmlDocPtr doc;
            xmlGenericErrorFunc oldErrorFunc = xmlGenericError;
            void *oldErrorContext = xmlGenericErrorContext;
            
            data = KWQServeSynchronousRequest(khtml::Cache::loader(), 
                                              globalDocLoader, job, finalURL, headers);
        
            xmlSetGenericErrorFunc(0, parseErrorFunc);
            // We don't specify an encoding here. Neither Gecko nor WinIE respects
            // the encoding specified in the HTTP headers.
            doc = xmlReadMemory(data.data(), data.size(), (const char *)uri, 0, options);
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

static const char **xsltParamArrayFromQDict(QDict<DOMString> parameters)
{
    if (parameters.count())
        return 0;
    const char **parameterArray = (const char **)malloc(((parameters.count() * 2) + 1) * sizeof(char *));

    QDictIterator<DOMString> it(parameters);
    unsigned index = 0;
    for (it.toFirst(); it.current(); ++it) {
        parameterArray[++index] = strdup(it.currentKey().utf8().data());
        parameterArray[++index] = strdup(it.current()->qstring().utf8().data());
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
        free((void *)*(temp++));
        free((void *)*(temp++));
    }
    free(params);
}


SharedPtr<DocumentImpl> XSLTProcessorImpl::createDocumentFromSource(const QString &sourceString, const QString &sourceMIMEType, NodeImpl *sourceNode, KHTMLView *view)
{
    SharedPtr<DocumentImpl> ownerDocument = sourceNode->getDocument();
    bool sourceIsDocument = (sourceNode == ownerDocument.get());
    QString documentSource = sourceString;

    SharedPtr<DocumentImpl> result;
    if (sourceMIMEType == "text/html")
        result = ownerDocument->impl()->createHTMLDocument(view);
    else {
        result = ownerDocument->impl()->createDocument(view);
        if (sourceMIMEType == "text/plain")
            transformTextStringToXHTMLDocumentString(documentSource);
    }
    
    result->docLoader()->setShowAnimations(ownerDocument->docLoader()->showAnimations());
    
    // Before parsing, we need to save & detach the old document and get the new document
    // in place. We have to do this only if we're rendering the result document.
    if (view) {
        view->clear();
        result->setTransformSourceDocument(view->part()->xmlDocImpl());
        view->part()->replaceDocImpl(result.get());
    }
    
    result->open();
    if (sourceIsDocument) {
        result->setURL(ownerDocument->URL());
        result->setBaseURL(ownerDocument->baseURL());
    }
    result->determineParseMode(documentSource); // Make sure we parse in the correct mode.
    result->write(documentSource);
    result->finishParsing();
    result->setParsing(false);
    if (view)
        view->part()->checkCompleted();
    else
        result->close(); // FIXME: Even viewless docs can load subresources. onload will fire too early.
                         // This is probably a bug in XMLHttpRequestObjects as well.
    return result;
}

static inline SharedPtr<DocumentFragmentImpl> createFragmentFromSource(QString sourceString, QString sourceMIMEType, NodeImpl *sourceNode, DocumentImpl *ouputDoc)
{
    SharedPtr<DocumentFragmentImpl> fragment = new DocumentFragmentImpl(ouputDoc);
    
    if (sourceMIMEType == "text/html")
        parseHTMLDocumentFragment(sourceString, fragment.get());
    else {
        if (sourceMIMEType == "text/plain")
            transformTextStringToXHTMLDocumentString(sourceString);
        bool successfulParse = parseXMLDocumentFragment(sourceString, fragment.get(), ouputDoc->documentElement());
        if (!successfulParse)
            return 0;
    }
    
    // FIXME: Do we need to mess with URLs here?
        
    return fragment;
}

static xsltStylesheetPtr xsltStylesheetPointer(SharedPtr<XSLStyleSheetImpl> &cachedStylesheet, NodeImpl *stylesheetRootNode)
{
    if (!cachedStylesheet && stylesheetRootNode) {
        cachedStylesheet = new XSLStyleSheetImpl(stylesheetRootNode->parent() ? stylesheetRootNode->parent() : stylesheetRootNode);
        cachedStylesheet->parseString(createMarkup(stylesheetRootNode));
    }
    
    if (!cachedStylesheet || !cachedStylesheet->document())
        return 0;
    
    return cachedStylesheet->compileStyleSheet();
}

static inline xmlDocPtr xmlDocPtrFromNode(NodeImpl *sourceNode)
{
    SharedPtr<DocumentImpl> ownerDocument = sourceNode->getDocument();
    bool sourceIsDocument = (sourceNode == ownerDocument.get());
    
    xmlDocPtr sourceDoc = 0;
    if (sourceIsDocument)
        sourceDoc = (xmlDocPtr)ownerDocument->transformSource();
    if (!sourceDoc)
        sourceDoc = (xmlDocPtr)xmlDocPtrForString(createMarkup(sourceNode), sourceIsDocument ? ownerDocument->URL() : QString());
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

bool XSLTProcessorImpl::transformToString(NodeImpl *sourceNode, QString &mimeType, QString &resultString)
{
    SharedPtr<DocumentImpl> ownerDocument = sourceNode->getDocument();
    SharedPtr<XSLStyleSheetImpl> cachedStylesheet = m_stylesheet;
    
    setXSLTLoadCallBack(stylesheetLoadFunc, this, ownerDocument->docLoader());
    xsltStylesheetPtr sheet = xsltStylesheetPointer(cachedStylesheet, m_stylesheetRootNode.get());
    if (!sheet) {
        setXSLTLoadCallBack(0, 0, 0);
        return false;
    }
    cachedStylesheet->clearDocuments();
    
    xmlDocPtr sourceDoc = xmlDocPtrFromNode(sourceNode);
    const char **params = xsltParamArrayFromQDict(m_parameters);
    xmlDocPtr resultDoc = xsltApplyStylesheet(sheet, sourceDoc, params);
    freeXsltParamArray(params);
    
    setXSLTLoadCallBack(0, 0, 0);
    
    if (!saveResultToString(resultDoc, sheet, resultString))
        return false;
    
    mimeType = resultMIMEType(resultDoc, sheet);
    
    xsltFreeStylesheet(sheet);
    xmlFreeDoc(resultDoc);
    
    return true;
}

SharedPtr<DocumentImpl> XSLTProcessorImpl::transformToDocument(NodeImpl *sourceNode)
{
    QString resultMIMEType;
    QString resultString;
    if (!transformToString(sourceNode, resultMIMEType, resultString))
        return 0;
    return createDocumentFromSource(resultString, resultMIMEType, sourceNode);
}

SharedPtr<DocumentFragmentImpl> XSLTProcessorImpl::transformToFragment(NodeImpl *sourceNode, DocumentImpl *outputDoc)
{
    QString resultMIMEType;
    QString resultString;
    if (!transformToString(sourceNode, resultMIMEType, resultString))
        return 0;
    return createFragmentFromSource(resultString, resultMIMEType, sourceNode, outputDoc);
}

void XSLTProcessorImpl::setParameter(DOMStringImpl *namespaceURI, DOMStringImpl *localName, DOMStringImpl *value)
{
    // FIXME: namespace support?
    m_parameters.replace(DOMString(localName).qstring(), new DOMString(value));
}

SharedPtr<DOMStringImpl> XSLTProcessorImpl::getParameter(DOMStringImpl *namespaceURI, DOMStringImpl *localName) const
{
    // FIXME: namespace support?
    if (DOMString *value = m_parameters.find(DOMString(localName).qstring()))
        return value->impl();
    return 0;
}

void XSLTProcessorImpl::removeParameter(DOMStringImpl *namespaceURI, DOMStringImpl *localName)
{
    // FIXME: namespace support?
    m_parameters.remove(DOMString(localName).qstring());
}

}

#endif
