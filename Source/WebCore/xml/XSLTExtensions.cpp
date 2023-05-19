/**
 * Copyright (C) 2001-2002 Thomas Broyer, Charlie Bozeman and Daniel Veillard.
 * Copyright (C) 2007 Alexey Proskuryakov <ap@webkit.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is fur-
 * nished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FIT-
 * NESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CON-
 * NECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the name of the authors shall not
 * be used in advertising or otherwise to promote the sale, use or other deal-
 * ings in this Software without prior written authorization from him.
 */

#include "config.h"

#if ENABLE(XSLT)
#include "XSLTExtensions.h"

#include <libxml/xpathInternals.h>

#include <libxslt/xsltutils.h>
#include <libxslt/extensions.h>
#include <libxslt/extra.h>

#if OS(DARWIN) && !PLATFORM(GTK)
#include "SoftLinkLibxslt.h"

static void xsltTransformErrorTrampoline(xsltTransformContextPtr context, xsltStylesheetPtr style, xmlNodePtr node, const char* message)
{
    static void (*xsltTransformErrorPointer)(xsltTransformContextPtr, xsltStylesheetPtr, xmlNodePtr, const char*, ...) WTF_ATTRIBUTE_PRINTF(4, 5);
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        xsltTransformErrorPointer = reinterpret_cast<void (*)(xsltTransformContextPtr, xsltStylesheetPtr, xmlNodePtr, const char*, ...)>(dlsym(WebCore::libxsltLibrary(), "xsltTransformError"));
    });
    xsltTransformErrorPointer(context, style, node, "%s", message);
}

#define xsltTransformError xsltTransformErrorTrampoline

#endif

namespace WebCore {

// FIXME: This code is taken from libexslt v1.1.35; should sync with newer versions.
static void exsltNodeSetFunction(xmlXPathParserContextPtr ctxt, int nargs)
{
    xmlDocPtr fragment;
    xsltTransformContextPtr tctxt = xsltXPathGetTransformContext(ctxt);
    xmlNodePtr txt;
    xmlChar *strval;
    xmlXPathObjectPtr obj;

    if (nargs != 1) {
        xmlXPathSetArityError(ctxt);
        return;
    }

    if (xmlXPathStackIsNodeSet(ctxt)) {
        xsltFunctionNodeSet(ctxt, nargs);
        return;
    }

    /*
     * SPEC EXSLT:
     * "You can also use this function to turn a string into a text
     * node, which is helpful if you want to pass a string to a
     * function that only accepts a node-set."
     */
    fragment = xsltCreateRVT(tctxt);
    if (!fragment) {
        xsltTransformError(tctxt, nullptr, tctxt->inst,
            "WebCore::exsltNodeSetFunction: Failed to create a tree fragment.\n");
        tctxt->state = XSLT_STATE_STOPPED;
        return;
    }
    xsltRegisterLocalRVT(tctxt, fragment);

    strval = xmlXPathPopString(ctxt);

    txt = xmlNewDocText(fragment, strval);
    xmlAddChild(reinterpret_cast<xmlNodePtr>(fragment), txt);
    obj = xmlXPathNewNodeSet(txt);

    // FIXME: It might be helpful to push any errors from xmlXPathNewNodeSet
    // up to the Javascript Console.
    if (!obj) {
        xsltTransformError(tctxt, nullptr, tctxt->inst,
            "WebCore::exsltNodeSetFunction: Failed to create a node set object.\n");
        tctxt->state = XSLT_STATE_STOPPED;
    }
    if (strval != NULL)
        xmlFree(strval);

    valuePush(ctxt, obj);
}

void registerXSLTExtensions(xsltTransformContextPtr ctxt)
{
    xsltRegisterExtFunction(ctxt, (const xmlChar*)"node-set", (const xmlChar*)"http://exslt.org/common", exsltNodeSetFunction);
}

}

#endif
