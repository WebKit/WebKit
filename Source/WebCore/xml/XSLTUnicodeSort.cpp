/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "config.h"
#include "XSLTUnicodeSort.h"

#if ENABLE(XSLT)

#include <libxslt/templates.h>
#include <libxslt/xsltutils.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Collator.h>

#if OS(DARWIN) && !PLATFORM(GTK)
#include "SoftLinkLibxslt.h"

static void xsltTransformErrorTrampoline(xsltTransformContextPtr, xsltStylesheetPtr, xmlNodePtr, const char* message, ...) WTF_ATTRIBUTE_PRINTF(4, 5);

void xsltTransformErrorTrampoline(xsltTransformContextPtr context, xsltStylesheetPtr style, xmlNodePtr node, const char* message, ...)
{
    va_list args;
    va_start(args, message);

    va_list preflightArgs;
    va_copy(preflightArgs, args);
    size_t stringLength = vsnprintf(nullptr, 0, message, preflightArgs);
    va_end(preflightArgs);

    Vector<char, 1024> buffer(stringLength + 1);
    vsnprintf(buffer.data(), stringLength + 1, message, args);
    va_end(args);

    static void (*xsltTransformErrorPointer)(xsltTransformContextPtr, xsltStylesheetPtr, xmlNodePtr, const char*, ...) WTF_ATTRIBUTE_PRINTF(4, 5)
        = reinterpret_cast<void (*)(xsltTransformContextPtr, xsltStylesheetPtr, xmlNodePtr, const char*, ...)>(dlsym(WebCore::libxsltLibrary(), "xsltTransformError"));
    xsltTransformErrorPointer(context, style, node, "%s", buffer.data());
}

#define xsltTransformError xsltTransformErrorTrampoline

#endif

namespace WebCore {

// Based on default implementation from libxslt 1.1.22 and xsltICUSort.c example.
void xsltUnicodeSortFunction(xsltTransformContextPtr ctxt, xmlNodePtr *sorts, int nbsorts)
{
#ifdef XSLT_REFACTORED
    xsltStyleItemSortPtr comp;
#else
    const xsltStylePreComp* comp;
#endif
    xmlXPathObjectPtr *resultsTab[XSLT_MAX_SORT];
    xmlXPathObjectPtr *results = NULL, *res;
    xmlNodeSetPtr list = NULL;
    int len = 0;
    int i, j, incr;
    int tst;
    int depth;
    xmlNodePtr node;
    xmlXPathObjectPtr tmp;    
    int number[XSLT_MAX_SORT], desc[XSLT_MAX_SORT];

    if ((ctxt == NULL) || (sorts == NULL) || (nbsorts <= 0) ||
        (nbsorts >= XSLT_MAX_SORT))
        return;
    if (sorts[0] == NULL)
        return;
    comp = static_cast<xsltStylePreComp*>(sorts[0]->psvi);
    if (comp == NULL)
        return;

    list = ctxt->nodeList;
    if ((list == NULL) || (list->nodeNr <= 1))
        return; /* nothing to do */

    for (j = 0; j < nbsorts; j++) {
        comp = static_cast<xsltStylePreComp*>(sorts[j]->psvi);
        if ((comp->stype == NULL) && (comp->has_stype != 0)) {
            xmlChar* stype =
                xsltEvalAttrValueTemplate(ctxt, sorts[j], (const xmlChar *) "data-type", XSLT_NAMESPACE);
            number[j] = 0;
            if (stype) {
                if (xmlStrEqual(stype, reinterpret_cast<const xmlChar*>("text"))) {
                    // number[j] already zero.
                } else if (xmlStrEqual(stype, reinterpret_cast<const xmlChar*>("number")))
                    number[j] = 1;
                else {
                    xsltTransformError(ctxt, NULL, sorts[j],
                          "xsltDoSortFunction: no support for data-type = %s\n",
                          stype);
                }
                xmlFree(stype);
            }
        } else {
            number[j] = comp->number;
        }
        if ((comp->order == NULL) && (comp->has_order != 0)) {
            xmlChar* order = xsltEvalAttrValueTemplate(ctxt, sorts[j],
                                                       reinterpret_cast<const xmlChar*>("order"),
                                                       XSLT_NAMESPACE);
            desc[j] = 0;
            if (order) {
                if (xmlStrEqual(order, reinterpret_cast<const xmlChar*>("ascending"))) {
                    // desc[j] already zero.
                } else if (xmlStrEqual(order, reinterpret_cast<const xmlChar*>("descending")))
                    desc[j] = 1;
                else {
                    xsltTransformError(ctxt, NULL, sorts[j],
                             "xsltDoSortFunction: invalid value %s for order\n",
                             order);
                }
                xmlFree(order);
            }
        } else {
            desc[j] = comp->descending;
        }
    }

    len = list->nodeNr;

    resultsTab[0] = xsltComputeSortResult(ctxt, sorts[0]);
    for (i = 1;i < XSLT_MAX_SORT;i++)
        resultsTab[i] = NULL;

    results = resultsTab[0];

    comp = static_cast<xsltStylePreComp*>(sorts[0]->psvi);
    if (results == NULL)
        return;

    // We are passing a language identifier to a function that expects a locale identifier.
    // The implementation of Collator should be lenient, and accept both "en-US" and "en_US", for example.
    // This lets an author specify sorting rules, e.g. "de_DE@collation=phonebook", which isn't
    // possible with language alone.
    Collator collator(comp->has_lang ? reinterpret_cast<const char*>(comp->lang) : "en", comp->lower_first);

    /* Shell's sort of node-set */
    for (incr = len / 2; incr > 0; incr /= 2) {
        for (i = incr; i < len; i++) {
            j = i - incr;
            if (results[i] == NULL)
                continue;
            
            while (j >= 0) {
                if (results[j] == NULL)
                    tst = 1;
                else {
                    if (number[0]) {
                        /* We make NaN smaller than number in accordance
                           with XSLT spec */
                        if (xmlXPathIsNaN(results[j]->floatval)) {
                            if (xmlXPathIsNaN(results[j + incr]->floatval))
                                tst = 0;
                            else
                                tst = -1;
                        } else if (xmlXPathIsNaN(results[j + incr]->floatval))
                            tst = 1;
                        else if (results[j]->floatval ==
                                results[j + incr]->floatval)
                            tst = 0;
                        else if (results[j]->floatval > 
                                results[j + incr]->floatval)
                            tst = 1;
                        else tst = -1;
                    } else
                        tst = collator.collateUTF8(reinterpret_cast<const char*>(results[j]->stringval), reinterpret_cast<const char*>(results[j + incr]->stringval));
                    if (desc[0])
                        tst = -tst;
                }
                if (tst == 0) {
                    /*
                     * Okay we need to use multi level sorts
                     */
                    depth = 1;
                    while (depth < nbsorts) {
                        if (sorts[depth] == NULL)
                            break;
                        comp = static_cast<xsltStylePreComp*>(sorts[depth]->psvi);
                        if (comp == NULL)
                            break;

                        /*
                         * Compute the result of the next level for the
                         * full set, this might be optimized ... or not
                         */
                        if (resultsTab[depth] == NULL) 
                            resultsTab[depth] = xsltComputeSortResult(ctxt,
                                                        sorts[depth]);
                        res = resultsTab[depth];
                        if (res == NULL) 
                            break;
                        if (res[j] == NULL) {
                            if (res[j+incr] != NULL)
                                tst = 1;
                        } else {
                            if (number[depth]) {
                                /* We make NaN smaller than number in
                                   accordance with XSLT spec */
                                if (xmlXPathIsNaN(res[j]->floatval)) {
                                    if (xmlXPathIsNaN(res[j +
                                                    incr]->floatval))
                                        tst = 0;
                                    else
                                        tst = -1;
                                } else if (xmlXPathIsNaN(res[j + incr]->
                                                floatval))
                                    tst = 1;
                                else if (res[j]->floatval == res[j + incr]->
                                                floatval)
                                    tst = 0;
                                else if (res[j]->floatval > 
                                        res[j + incr]->floatval)
                                    tst = 1;
                                else tst = -1;
                            } else
                                tst = collator.collateUTF8(reinterpret_cast<const char*>(res[j]->stringval), reinterpret_cast<const char*>(res[j + incr]->stringval));
                            if (desc[depth])
                                tst = -tst;
                        }

                        /*
                         * if we still can't differenciate at this level
                         * try one level deeper.
                         */
                        if (tst != 0)
                            break;
                        depth++;
                    }
                }
                if (tst == 0) {
                    tst = results[j]->index > results[j + incr]->index;
                }
                if (tst > 0) {
                    tmp = results[j];
                    results[j] = results[j + incr];
                    results[j + incr] = tmp;
                    node = list->nodeTab[j];
                    list->nodeTab[j] = list->nodeTab[j + incr];
                    list->nodeTab[j + incr] = node;
                    depth = 1;
                    while (depth < nbsorts) {
                        if (sorts[depth] == NULL)
                            break;
                        if (resultsTab[depth] == NULL)
                            break;
                        res = resultsTab[depth];
                        tmp = res[j];
                        res[j] = res[j + incr];
                        res[j + incr] = tmp;
                        depth++;
                    }
                    j -= incr;
                } else
                    break;
            }
        }
    }

    for (j = 0; j < nbsorts; j++) {
        comp = static_cast<xsltStylePreComp*>(sorts[j]->psvi);
        if (resultsTab[j] != NULL) {
            for (i = 0;i < len;i++)
                xmlXPathFreeObject(resultsTab[j][i]);
            xmlFree(resultsTab[j]);
        }
    }
}

}

#endif
