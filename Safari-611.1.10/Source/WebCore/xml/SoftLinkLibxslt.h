/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include <libxslt/imports.h>
#include <libxslt/security.h>
#include <libxslt/templates.h>
#include <libxslt/variables.h>
#include <libxslt/xsltutils.h>

#include <wtf/SoftLinking.h>

SOFT_LINK_LIBRARY_FOR_HEADER(WebCore, libxslt)

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltFreeStylesheet, void, (xsltStylesheetPtr sheet), (sheet))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltFreeTransformContext, void, (xsltTransformContextPtr ctxt), (ctxt))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltNewTransformContext, xsltTransformContextPtr, (xsltStylesheetPtr style, xmlDocPtr doc), (style, doc))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltApplyStylesheetUser, xmlDocPtr, (xsltStylesheetPtr style, xmlDocPtr doc, const char** params, const char* output, FILE* profile, xsltTransformContextPtr userCtxt), (style, doc, params, output, profile, userCtxt))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltQuoteUserParams, int, (xsltTransformContextPtr ctxt, const char** params), (ctxt, params))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltSetCtxtSortFunc, void, (xsltTransformContextPtr ctxt, xsltSortFunc handler), (ctxt, handler))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltSetLoaderFunc, void, (xsltDocLoaderFunc f), (f))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltSaveResultTo, int, (xmlOutputBufferPtr buf, xmlDocPtr result, xsltStylesheetPtr style), (buf, result, style))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltNextImport, xsltStylesheetPtr, (xsltStylesheetPtr style), (style))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltNewSecurityPrefs, xsltSecurityPrefsPtr, (), ())
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltFreeSecurityPrefs, void, (xsltSecurityPrefsPtr sec), (sec))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltSetSecurityPrefs, int, (xsltSecurityPrefsPtr sec, xsltSecurityOption option, xsltSecurityCheck func), (sec, option, func))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltSetCtxtSecurityPrefs, int, (xsltSecurityPrefsPtr sec, xsltTransformContextPtr ctxt), (sec, ctxt))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltSecurityForbid, int, (xsltSecurityPrefsPtr sec, xsltTransformContextPtr ctxt, const char* value), (sec, ctxt, value))

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltGetNsProp, xmlChar *, (xmlNodePtr node, const xmlChar *name, const xmlChar *nameSpace), (node, name, nameSpace))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltParseStylesheetDoc, xsltStylesheetPtr, (xmlDocPtr doc), (doc))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltLoadStylesheetPI, xsltStylesheetPtr, (xmlDocPtr doc), (doc))

SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltRegisterExtFunction, int, (xsltTransformContextPtr ctxt, const xmlChar *name, const xmlChar *URI, xmlXPathFunction function), (ctxt, name, URI, function))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltFunctionNodeSet, void, (xmlXPathParserContextPtr ctxt, int nargs), (ctxt, nargs))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltComputeSortResult, xmlXPathObjectPtr*, (xsltTransformContextPtr ctxt, xmlNodePtr sort), (ctxt, sort))
SOFT_LINK_FUNCTION_FOR_HEADER(WebCore, libxslt, xsltEvalAttrValueTemplate, xmlChar*, (xsltTransformContextPtr ctxt, xmlNodePtr node, const xmlChar *name, const xmlChar *ns), (ctxt, node, name, ns))

SOFT_LINK_VARIABLE_FOR_HEADER(WebCore, libxslt, xsltMaxDepth, int);
#define xsltMaxDepth get_libxslt_xsltMaxDepth()

#endif // OS(DARWIN) && !PLATFORM(GTK)
