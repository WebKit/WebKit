/*
 * This file is part of the XSL implementation.
 *
 * Copyright (C) 2004-2024 Apple, Inc. All rights reserved.
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

#include "CachedResourceLoader.h"
#include "DocumentInlines.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "LocalFrame.h"
#include "OriginAccessPatterns.h"
#include "Page.h"
#include "PageConsoleClient.h"
#include "ResourceError.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "SharedBuffer.h"
#include "TransformSource.h"
#include "XMLDocumentParser.h"
#include "XMLDocumentParserScope.h"
#include "XSLTExtensions.h"
#include "XSLTUnicodeSort.h"
#include "markup.h"
#include <libxslt/imports.h>
#include <libxslt/security.h>
#include <libxslt/variables.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltutils.h>
#include <wtf/Assertions.h>
#include <wtf/CheckedArithmetic.h>

namespace WebCore {

void XSLTProcessor::genericErrorFunc(void*, const char*, ...)
{
    // It would be nice to do something with this error message.
}

#if LIBXML_VERSION >= 21200
void XSLTProcessor::parseErrorFunc(void* userData, const xmlError* error)
#else
void XSLTProcessor::parseErrorFunc(void* userData, xmlError* error)
#endif
{
    PageConsoleClient* console = static_cast<PageConsoleClient*>(userData);
    if (!console)
        return;

    MessageLevel level;
    switch (error->level) {
    case XML_ERR_NONE:
        level = MessageLevel::Debug;
        break;
    case XML_ERR_WARNING:
        level = MessageLevel::Warning;
        break;
    case XML_ERR_ERROR:
    case XML_ERR_FATAL:
    default:
        level = MessageLevel::Error;
        break;
    }

    // xmlError->int2 is the column number of the error or 0 if N/A.
    console->addMessage(MessageSource::XML, level, String::fromLatin1(error->message), String::fromLatin1(error->file), error->line, error->int2);
}

// FIXME: There seems to be no way to control the ctxt pointer for loading here, thus we have globals.
static XSLTProcessor* globalProcessor = nullptr;
static WeakPtr<CachedResourceLoader>& globalCachedResourceLoader()
{
    static NeverDestroyed<WeakPtr<CachedResourceLoader>> globalCachedResourceLoader;
    return globalCachedResourceLoader;
}
static xmlDocPtr docLoaderFunc(const xmlChar* uri,
                               xmlDictPtr,
                               int options,
                               void* ctxt,
                               xsltLoadType type)
{
    if (!globalProcessor)
        return nullptr;

    switch (type) {
    case XSLT_LOAD_DOCUMENT: {
        xsltTransformContextPtr context = (xsltTransformContextPtr)ctxt;
        xmlChar* base = xmlNodeGetBase(context->document->doc, context->node);
        URL url(URL({ }, String::fromLatin1(byteCast<char>(base))), String::fromLatin1(byteCast<char>(uri)));
        xmlFree(base);
        ResourceError error;
        ResourceResponse response;

        RefPtr<SharedBuffer> data;
        RefPtr cachedResourceLoader = globalCachedResourceLoader().get();

        bool requestAllowed = cachedResourceLoader && cachedResourceLoader->frame() && cachedResourceLoader->document()->protectedSecurityOrigin()->canRequest(url, OriginAccessPatternsForWebProcess::singleton());
        if (requestAllowed) {
            FetchOptions options;
            options.mode = FetchOptions::Mode::SameOrigin;
            options.credentials = FetchOptions::Credentials::Include;
            cachedResourceLoader->frame()->loader().loadResourceSynchronously(url, ClientCredentialPolicy::MayAskClientForCredentials, options, { }, error, response, data);
            if (error.isNull())
                requestAllowed = cachedResourceLoader->document()->protectedSecurityOrigin()->canRequest(response.url(), OriginAccessPatternsForWebProcess::singleton());
            else if (data)
                data = nullptr;
        }
        if (!requestAllowed) {
            if (data)
                data = nullptr;
            if (cachedResourceLoader)
                cachedResourceLoader->printAccessDeniedMessage(url);
        }

        // Return early if nothing to parse.
        if (!data || !data->size())
            return nullptr;

        PageConsoleClient* console = nullptr;
        auto* frame = globalProcessor->xslStylesheet()->ownerDocument()->frame();
        if (frame && frame->page())
            console = &frame->page()->console();
        XMLDocumentParserScope scope(cachedResourceLoader.get(), XSLTProcessor::genericErrorFunc, XSLTProcessor::parseErrorFunc, console);

        // We don't specify an encoding here. Neither Gecko nor WinIE respects
        // the encoding specified in the HTTP headers.
        auto dataSpan = byteCast<char>(data->span());
        return xmlReadMemory(dataSpan.data(), dataSpan.size(), byteCast<char>(uri), nullptr, options);
    }
    case XSLT_LOAD_STYLESHEET:
        return globalProcessor->xslStylesheet()->locateStylesheetSubResource(((xsltStylesheetPtr)ctxt)->doc, uri);
    default:
        break;
    }

    return nullptr;
}

static inline void setXSLTLoadCallBack(xsltDocLoaderFunc func, XSLTProcessor* processor, CachedResourceLoader* cachedResourceLoader)
{
    xsltSetLoaderFunc(func);
    globalProcessor = processor;
    globalCachedResourceLoader() = cachedResourceLoader;
}

static int writeToStringBuilder(void* context, const char* buffer, int length)
{
    auto& builder = *static_cast<StringBuilder*>(context);
    if (!length)
        return 0;
    auto checkedString = WTF::Unicode::checkUTF8({ byteCast<char8_t>(buffer), static_cast<size_t>(length) });
    if (checkedString.characters.empty())
        return -1;
    builder.append(checkedString);
    return checkedString.characters.size();
}

static bool saveResultToString(xmlDocPtr resultDoc, xsltStylesheetPtr sheet, String& resultString)
{
    xmlOutputBufferPtr outputBuf = xmlAllocOutputBuffer(nullptr);
    if (!outputBuf)
        return false;

    StringBuilder resultBuilder;
    outputBuf->context = &resultBuilder;
    outputBuf->writecallback = writeToStringBuilder;

    int retval = xsltSaveResultTo(outputBuf, resultDoc, sheet);
    xmlOutputBufferClose(outputBuf);
    if (retval < 0)
        return false;

    // Workaround for <http://bugzilla.gnome.org/show_bug.cgi?id=495668>: libxslt appends an extra line feed to the result.
    if (resultBuilder.length() > 0 && resultBuilder[resultBuilder.length() - 1] == '\n')
        resultBuilder.shrink(resultBuilder.length() - 1);

    resultString = resultBuilder.toString();

    return true;
}

static const char** xsltParamArrayFromParameterMap(XSLTProcessor::ParameterMap& parameters)
{
    if (parameters.isEmpty())
        return 0;

    auto size = (((Checked<size_t>(parameters.size()) * 2U) + 1U) * sizeof(char*)).value();
    auto** parameterArray = static_cast<const char**>(fastMalloc(size));

    size_t index = 0;
    for (auto& parameter : parameters) {
        parameterArray[index++] = fastStrDup(parameter.key.utf8().data());
        parameterArray[index++] = fastStrDup(parameter.value.utf8().data());
    }
    parameterArray[index] = nullptr;

#if !PLATFORM(WIN) && !PLATFORM(COCOA)
    RELEASE_ASSERT(index <= std::numeric_limits<int>::max());
#endif

    return parameterArray;
}

static void freeXsltParamArray(const char** params)
{
    const char** temp = params;
    if (!params)
        return;

    while (*temp) {
        fastFree((void*)*(temp++));
        fastFree((void*)*(temp++));
    }
    fastFree(params);
}

static xsltStylesheetPtr xsltStylesheetPointer(RefPtr<XSLStyleSheet>& cachedStylesheet, Node* stylesheetRootNode)
{
    if (!cachedStylesheet && stylesheetRootNode) {
        cachedStylesheet = XSLStyleSheet::createForXSLTProcessor(stylesheetRootNode->parentNode() ? stylesheetRootNode->parentNode() : stylesheetRootNode,
            stylesheetRootNode->document().url().string(),
            stylesheetRootNode->document().url()); // FIXME: Should we use baseURL here?

        // According to Mozilla documentation, the node must be a Document node, an xsl:stylesheet or xsl:transform element.
        // But we just use text content regardless of node type.
        cachedStylesheet->parseString(serializeFragment(*stylesheetRootNode, SerializedNodes::SubtreeIncludingNode));
    }

    if (!cachedStylesheet || !cachedStylesheet->document())
        return 0;

    return cachedStylesheet->compileStyleSheet();
}

static inline xmlDocPtr xmlDocPtrFromNode(Node& sourceNode, bool& shouldDelete)
{
    Ref<Document> ownerDocument(sourceNode.document());
    bool sourceIsDocument = (&sourceNode == &ownerDocument.get());

    xmlDocPtr sourceDoc = nullptr;
    if (sourceIsDocument && ownerDocument->transformSource())
        sourceDoc = ownerDocument->transformSource()->platformSource();
    if (!sourceDoc) {
        sourceDoc = xmlDocPtrForString(ownerDocument->cachedResourceLoader(), serializeFragment(sourceNode, SerializedNodes::SubtreeIncludingNode),
            sourceIsDocument ? ownerDocument->url().string() : String());
        shouldDelete = sourceDoc;
    }
    return sourceDoc;
}

static inline String resultMIMEType(xmlDocPtr resultDoc, xsltStylesheetPtr sheet)
{
    // There are three types of output we need to be able to deal with:
    // HTML (create an HTML document), XML (create an XML document),
    // and text (wrap in a <pre> and create an XML document).

    const xmlChar* resultType = nullptr;
    XSLT_GET_IMPORT_PTR(resultType, sheet, method);
    if (!resultType && resultDoc->type == XML_HTML_DOCUMENT_NODE)
        resultType = (const xmlChar*)"html";

    if (xmlStrEqual(resultType, (const xmlChar*)"html"))
        return textHTMLContentTypeAtom();
    if (xmlStrEqual(resultType, (const xmlChar*)"text"))
        return textPlainContentTypeAtom();

    return applicationXMLContentTypeAtom();
}

bool XSLTProcessor::transformToString(Node& sourceNode, String& mimeType, String& resultString, String& resultEncoding)
{
    Ref<Document> ownerDocument(sourceNode.document());

    setXSLTLoadCallBack(docLoaderFunc, this, &ownerDocument->cachedResourceLoader());
    xsltStylesheetPtr sheet = xsltStylesheetPointer(m_stylesheet, m_stylesheetRootNode.get());
    if (!sheet) {
        setXSLTLoadCallBack(nullptr, nullptr, nullptr);
        m_stylesheet = nullptr;
        return false;
    }
    m_stylesheet->clearDocuments();

    int origXsltMaxDepth = xsltMaxDepth;
    xsltMaxDepth = 1000;

    xmlChar* origMethod = sheet->method;
    if (!origMethod && mimeType == textHTMLContentTypeAtom())
        sheet->method = byteCast<xmlChar>(const_cast<char*>("html"));

    bool success = false;
    bool shouldFreeSourceDoc = false;
    if (xmlDocPtr sourceDoc = xmlDocPtrFromNode(sourceNode, shouldFreeSourceDoc)) {
        // The XML declaration would prevent parsing the result as a fragment, and it's not needed even for documents,
        // as the result of this function is always immediately parsed.
        sheet->omitXmlDeclaration = true;

        xsltTransformContextPtr transformContext = xsltNewTransformContext(sheet, sourceDoc);
        registerXSLTExtensions(transformContext);

        xsltSecurityPrefsPtr securityPrefs = xsltNewSecurityPrefs();
        // Read permissions are checked by docLoaderFunc.
        if (0 != xsltSetSecurityPrefs(securityPrefs, XSLT_SECPREF_WRITE_FILE, xsltSecurityForbid))
            CRASH();
        if (0 != xsltSetSecurityPrefs(securityPrefs, XSLT_SECPREF_CREATE_DIRECTORY, xsltSecurityForbid))
            CRASH();
        if (0 != xsltSetSecurityPrefs(securityPrefs, XSLT_SECPREF_WRITE_NETWORK, xsltSecurityForbid))
            CRASH();
        if (0 != xsltSetCtxtSecurityPrefs(securityPrefs, transformContext))
            CRASH();

        // <http://bugs.webkit.org/show_bug.cgi?id=16077>: XSLT processor <xsl:sort> algorithm only compares by code point.
        xsltSetCtxtSortFunc(transformContext, xsltUnicodeSortFunction);

        const char** params = xsltParamArrayFromParameterMap(m_parameters);
        xsltQuoteUserParams(transformContext, params);
        xmlDocPtr resultDoc = xsltApplyStylesheetUser(sheet, sourceDoc, nullptr, nullptr, nullptr, transformContext);

        xsltFreeTransformContext(transformContext);
        xsltFreeSecurityPrefs(securityPrefs);
        freeXsltParamArray(params);

        if (shouldFreeSourceDoc)
            xmlFreeDoc(sourceDoc);

        if ((success = saveResultToString(resultDoc, sheet, resultString))) {
            mimeType = resultMIMEType(resultDoc, sheet);
            resultEncoding = String::fromLatin1(byteCast<char>(resultDoc->encoding));
        }
        xmlFreeDoc(resultDoc);
    }

    sheet->method = origMethod;
    xsltMaxDepth = origXsltMaxDepth;
    setXSLTLoadCallBack(nullptr, nullptr, nullptr);
    xsltFreeStylesheet(sheet);
    m_stylesheet = nullptr;

    return success;
}

} // namespace WebCore

#endif // ENABLE(XSLT)
