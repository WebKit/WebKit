/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "FragmentDirectiveRangeFinder.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "Editor.h"
#include "FindOptions.h"
#include "SimpleRange.h"
#include "TextIterator.h"

namespace WebCore {
namespace FragmentDirectiveRangeFinder {

static SimpleRange collapseIfRootsDiffer(SimpleRange&& range)
{
    return &range.start.container->rootNode() == &range.end.container->rootNode()
        ? WTFMove(range) : SimpleRange { range.start, range.start };
}

static std::optional<SimpleRange> rangeOfString(const String& target, Document& document)
{
    if (target.isEmpty())
        return std::nullopt;

    auto searchRange = makeRangeSelectingNodeContents(document);
    
    FindOptions findOptions { DoNotRevealSelection };
    
    auto resultRange = collapseIfRootsDiffer(findPlainText(searchRange, target, findOptions));

    return resultRange.collapsed() ? std::nullopt : std::make_optional(resultRange);
}

const Vector<SimpleRange> rangesForFragments(Vector<ParsedTextDirective>& parsedTextDirectives, Document& document)
{
    Vector<SimpleRange> ranges;
    
    for (auto directive : parsedTextDirectives) {
        // FIXME: do full algorithm to find the exact text phrase, rather than just searching for the first place that the text is
        auto range = rangeOfString(directive.textStart, document);
        if (range && !range->collapsed())
            ranges.append(range.value());
    }
    
    return ranges;
}

}

} // WebCore
