/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once


#if OS(DARWIN) && !PLATFORM(GTK)

#include <libxslt/documents.h>
#include <libxslt/extensions.h>
#include <libxslt/extra.h>
#include <libxslt/imports.h>
#include <libxslt/security.h>
#include <libxslt/templates.h>
#include <libxslt/variables.h>
#include <libxslt/xsltutils.h>

#include <wtf/SoftLinking.h>

SOFT_LINK_LIBRARY_FOR_HEADER(WebCore, libxslt)

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltFreeStylesheet, void, (xsltStylesheetPtr sheet), (sheet))
#define xsltFreeStylesheet WebCore::softLink_libxslt_xsltFreeStylesheet
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltFreeTransformContext, void, (xsltTransformContextPtr ctxt), (ctxt))
#define xsltFreeTransformContext WebCore::softLink_libxslt_xsltFreeTransformContext
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltNewTransformContext, xsltTransformContextPtr, (xsltStylesheetPtr style, xmlDocPtr doc), (style, doc))
#define xsltNewTransformContext WebCore::softLink_libxslt_xsltNewTransformContext
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltApplyStylesheetUser, xmlDocPtr, (xsltStylesheetPtr style, xmlDocPtr doc, const char** params, const char* output, FILE* profile, xsltTransformContextPtr userCtxt), (style, doc, params, output, profile, userCtxt))
#define xsltApplyStylesheetUser WebCore::softLink_libxslt_xsltApplyStylesheetUser
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltQuoteUserParams, int, (xsltTransformContextPtr ctxt, const char** params), (ctxt, params))
#define xsltQuoteUserParams WebCore::softLink_libxslt_xsltQuoteUserParams
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltSetCtxtSortFunc, void, (xsltTransformContextPtr ctxt, xsltSortFunc handler), (ctxt, handler))
#define xsltSetCtxtSortFunc WebCore::softLink_libxslt_xsltSetCtxtSortFunc
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltSetLoaderFunc, void, (xsltDocLoaderFunc f), (f))
#define xsltSetLoaderFunc WebCore::softLink_libxslt_xsltSetLoaderFunc
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltSaveResultTo, int, (xmlOutputBufferPtr buf, xmlDocPtr result, xsltStylesheetPtr style), (buf, result, style))
#define xsltSaveResultTo WebCore::softLink_libxslt_xsltSaveResultTo
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltNextImport, xsltStylesheetPtr, (xsltStylesheetPtr style), (style))
#define xsltNextImport WebCore::softLink_libxslt_xsltNextImport
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltNewSecurityPrefs, xsltSecurityPrefsPtr, (), ())
#define xsltNewSecurityPrefs WebCore::softLink_libxslt_xsltNewSecurityPrefs
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltFreeSecurityPrefs, void, (xsltSecurityPrefsPtr sec), (sec))
#define xsltFreeSecurityPrefs WebCore::softLink_libxslt_xsltFreeSecurityPrefs
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltSetSecurityPrefs, int, (xsltSecurityPrefsPtr sec, xsltSecurityOption option, xsltSecurityCheck func), (sec, option, func))
#define xsltSetSecurityPrefs WebCore::softLink_libxslt_xsltSetSecurityPrefs
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltSetCtxtSecurityPrefs, int, (xsltSecurityPrefsPtr sec, xsltTransformContextPtr ctxt), (sec, ctxt))
#define xsltSetCtxtSecurityPrefs WebCore::softLink_libxslt_xsltSetCtxtSecurityPrefs
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltSecurityForbid, int, (xsltSecurityPrefsPtr sec, xsltTransformContextPtr ctxt, const char* value), (sec, ctxt, value))
#define xsltSecurityForbid WebCore::softLink_libxslt_xsltSecurityForbid

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltGetNsProp, xmlChar *, (xmlNodePtr node, const xmlChar *name, const xmlChar *nameSpace), (node, name, nameSpace))
#define xsltGetNsProp WebCore::softLink_libxslt_xsltGetNsProp
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltParseStylesheetDoc, xsltStylesheetPtr, (xmlDocPtr doc), (doc))
#define xsltParseStylesheetDoc WebCore::softLink_libxslt_xsltParseStylesheetDoc
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltLoadStylesheetPI, xsltStylesheetPtr, (xmlDocPtr doc), (doc))
#define xsltLoadStylesheetPI WebCore::softLink_libxslt_xsltLoadStylesheetPI

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltRegisterExtFunction, int, (xsltTransformContextPtr ctxt, const xmlChar *name, const xmlChar *URI, xmlXPathFunction function), (ctxt, name, URI, function))
#define xsltRegisterExtFunction WebCore::softLink_libxslt_xsltRegisterExtFunction
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltFunctionNodeSet, void, (xmlXPathParserContextPtr ctxt, int nargs), (ctxt, nargs))
#define xsltFunctionNodeSet WebCore::softLink_libxslt_xsltFunctionNodeSet
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltComputeSortResult, xmlXPathObjectPtr*, (xsltTransformContextPtr ctxt, xmlNodePtr sort), (ctxt, sort))
#define xsltComputeSortResult WebCore::softLink_libxslt_xsltComputeSortResult
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltEvalAttrValueTemplate, xmlChar*, (xsltTransformContextPtr ctxt, xmlNodePtr node, const xmlChar *name, const xmlChar *ns), (ctxt, node, name, ns))
#define xsltEvalAttrValueTemplate WebCore::softLink_libxslt_xsltEvalAttrValueTemplate

SOFT_LINK_VARIABLE_FOR_HEADER(WebCore, libxslt, xsltMaxDepth, int);
#define xsltMaxDepth get_libxslt_xsltMaxDepth()

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltCreateRVT, xmlDocPtr, (xsltTransformContextPtr ctxt), (ctxt))
#define xsltCreateRVT WebCore::softLink_libxslt_xsltCreateRVT
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltRegisterLocalRVT, int, (xsltTransformContextPtr ctxt, xmlDocPtr RVT), (ctxt, RVT))
#define xsltRegisterLocalRVT WebCore::softLink_libxslt_xsltRegisterLocalRVT
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltXPathGetTransformContext, xsltTransformContextPtr, (xmlXPathParserContextPtr ctxt), (ctxt))
#define xsltXPathGetTransformContext WebCore::softLink_libxslt_xsltXPathGetTransformContext

#endif // OS(DARWIN) && !PLATFORM(GTK)
