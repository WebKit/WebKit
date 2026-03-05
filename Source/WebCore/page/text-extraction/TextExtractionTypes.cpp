/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextExtractionTypes.h"

#include <wtf/TZoneMallocInlines.h>

namespace WebCore::TextExtraction {

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(Result);

static unsigned collateRecursive(Item& item, HashMap<FrameIdentifier, UniqueRef<Result>>& subFrameResults)
{
    if (subFrameResults.isEmpty())
        return 0;

    unsigned additionalTextLength = 0;

    if (auto iframe = item.dataAs<IFrameData>(); iframe && item.children.isEmpty()) {
        if (auto subFrameResult = subFrameResults.take(iframe->identifier)) {
            item.children = WTF::move(subFrameResult->rootItem.children);
            additionalTextLength += subFrameResult->visibleTextLength;
        }
    }

    for (auto& child : item.children)
        additionalTextLength += collateRecursive(child, subFrameResults);

    return additionalTextLength;
}

Result collatePageResults(PageResults&& results)
{
    auto additionalLength = collateRecursive(results.mainFrameResult.rootItem, results.subFrameResults);
    results.mainFrameResult.visibleTextLength += additionalLength;
    return WTF::move(results.mainFrameResult);
}

} // namespace WebCore::TextExtraction
