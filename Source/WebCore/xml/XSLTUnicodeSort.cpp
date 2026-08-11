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

#include <algorithm>
#include <array>
#include <libxslt/templates.h>
#include <libxslt/xsltutils.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Collator.h>

namespace WebCore {

// Based on default implementation from libxslt 1.1.22 and xsltICUSort.c example.
void xsltUnicodeSortFunction(xsltTransformContextPtr ctxt, xmlNodePtr* rawSorts, int nbsorts)
{
    if (!ctxt || !rawSorts || nbsorts <= 0 || nbsorts >= XSLT_MAX_SORT)
        return;

    auto sorts = unsafeMakeSpan(rawSorts, nbsorts);
    if (!sorts[0])
        return;

    auto comp = static_cast<xsltStylePreComp*>(sorts[0]->psvi);
    if (!comp)
        return;

    auto list = ctxt->nodeList;
    if (!list || list->nodeNr <= 1)
        return; /* nothing to do */

    std::array<int, XSLT_MAX_SORT> desc;
    std::array<int, XSLT_MAX_SORT> number;
    for (size_t j = 0; j < sorts.size(); ++j) {
        comp = static_cast<xsltStylePreComp*>(sorts[j]->psvi);
        if (!comp->stype && comp->has_stype) {
            auto* stype = xsltEvalAttrValueTemplate(ctxt, sorts[j], reinterpret_cast<const xmlChar*>("data-type"), XSLT_NAMESPACE);
            number[j] = 0;
            if (stype) {
                if (xmlStrEqual(stype, reinterpret_cast<const xmlChar*>("text"))) {
                    // number[j] already zero.
                } else if (xmlStrEqual(stype, reinterpret_cast<const xmlChar*>("number")))
                    number[j] = 1;
                else
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
                    xsltTransformError(ctxt, nullptr, sorts[j], "xsltDoSortFunction: no support for data-type = %s\n", stype);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
                xmlFree(stype);
            }
        } else
            number[j] = comp->number;
        if (!comp->order && comp->has_order) {
            auto* order = xsltEvalAttrValueTemplate(ctxt, sorts[j], reinterpret_cast<const xmlChar*>("order"), XSLT_NAMESPACE);
            desc[j] = 0;
            if (order) {
                if (xmlStrEqual(order, reinterpret_cast<const xmlChar*>("ascending"))) {
                    // desc[j] already zero.
                } else if (xmlStrEqual(order, reinterpret_cast<const xmlChar*>("descending")))
                    desc[j] = 1;
                else
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
                    xsltTransformError(ctxt, nullptr, sorts[j], "xsltDoSortFunction: invalid value %s for order\n", order);
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
                xmlFree(order);
            }
        } else
            desc[j] = comp->descending;
    }

    auto len = list->nodeNr;
    auto listNodes = unsafeMakeSpan(list->nodeTab, len);

    std::array<std::span<xmlXPathObjectPtr>, XSLT_MAX_SORT> resultsTab = { };
    resultsTab[0] = unsafeMakeSpan(xsltComputeSortResult(ctxt, sorts[0]), len);

    auto results = resultsTab[0];
    if (!results.data())
        return;

    comp = static_cast<xsltStylePreComp*>(sorts[0]->psvi);

    // We are passing a language identifier to a function that expects a locale identifier.
    // The implementation of Collator should be lenient, and accept both "en-US" and "en_US", for example.
    // This lets an author specify sorting rules, e.g. "de_DE@collation=phonebook", which isn't
    // possible with language alone.
    Collator collator(comp->has_lang ? byteCast<char>(comp->lang) : "en", comp->lower_first);

    int depth = 0;
    int tst = 0;
    std::span<xmlXPathObjectPtr> res;
    /* Shell's sort of node-set */
    for (int incr = len / 2; incr > 0; incr /= 2) {
        for (int i = incr; i < len; i++) {
            int j = i - incr;
            if (!results[i])
                continue;
            
            while (j >= 0) {
                if (!results[j])
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
                        tst = collator.collate(byteCast<char8_t>(results[j]->stringval), byteCast<char8_t>(results[j + incr]->stringval));
                    if (desc[0])
                        tst = -tst;
                }
                if (tst == 0) {
                    /*
                     * Okay we need to use multi level sorts
                     */
                    depth = 1;
                    while (depth < nbsorts) {
                        if (!sorts[depth])
                            break;
                        comp = static_cast<xsltStylePreComp*>(sorts[depth]->psvi);
                        if (!comp)
                            break;

                        /*
                         * Compute the result of the next level for the
                         * full set, this might be optimized ... or not
                         */
                        if (!resultsTab[depth].data())
                            resultsTab[depth] = unsafeMakeSpan(xsltComputeSortResult(ctxt, sorts[depth]), len);
                        res = resultsTab[depth];
                        if (!res.data())
                            break;
                        if (!res[j]) {
                            if (res[j + incr])
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
                                tst = collator.collate(byteCast<char8_t>(res[j]->stringval), byteCast<char8_t>(res[j + incr]->stringval));
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
                    auto tmp = results[j];
                    results[j] = results[j + incr];
                    results[j + incr] = tmp;
                    auto node = listNodes[j];
                    listNodes[j] = listNodes[j + incr];
                    listNodes[j + incr] = node;
                    depth = 1;
                    while (depth < nbsorts) {
                        if (!sorts[depth])
                            break;
                        if (!resultsTab[depth].data())
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

    for (size_t j = 0; j < sorts.size(); ++j) {
        if (resultsTab[j].data()) {
            for (int i = 0; i < len; ++i)
                xmlXPathFreeObject(resultsTab[j][i]);
            xmlFree(resultsTab[j].data());
        }
    }
}

}

#endif
