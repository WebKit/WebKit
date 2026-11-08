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

#pragma once

#include <WebCore/StyleCustomIdent.h>
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

namespace Style {
struct GridTemplateList;
}

namespace Layout {

// https://drafts.csswg.org/css-grid-1/#named-lines
// The lines of one axis of the explicit grid that carry a name, keyed by the name. The lines are 0-based and sorted
// in ascending order, so "the Nth line named X" (from either end) is an index into the span and "the
// next line named X after line L" is a binary search. Immutable once constructed.
// The names that grid-template-areas gives to lines ("foo-start" and "foo-end") are not in it yet; create() will take them too.
class NamedGridLines {
public:
    // The 0-based lines of the explicit grid that carry each line name.
    using NamedLines = HashMap<Style::CustomIdent, Vector<size_t>>;

    NamedGridLines() = default;

    // autoRepeatTracksCount, autoRepeatTruncatedByLimit and explicitTracksCount are what ExplicitGridResolver resolved for the axis.
    // Nothing calls this yet.
    static NamedGridLines create(const Style::GridTemplateList&, size_t autoRepeatTracksCount, bool autoRepeatTruncatedByLimit, size_t explicitTracksCount);

    // Empty when no line has the name.
    std::span<const size_t> lines(const Style::CustomIdent&) const;

private:
    // Sorts the lines of each name and removes the duplicates, since a name can be given to the same line more than once.
    explicit NamedGridLines(NamedLines&&);

    NamedLines m_namedLines;
};

} // namespace Layout
} // namespace WebCore
