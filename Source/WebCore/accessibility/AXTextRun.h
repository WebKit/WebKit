/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(AX_THREAD_TEXT_APIS)

#include <CoreText/CTFont.h>
#include <WebCore/FloatRect.h>
#include <WebCore/TextAffinity.h>
#include <WebCore/TextFlags.h>
#include <wtf/FixedVector.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

class RenderBlock;

struct AXTextRunLineID {
    // Do not dereference, for comparison to other AXTextRunLineIDs only.
    void* containingBlock { nullptr };
    size_t lineIndex { 0 };

    AXTextRunLineID() = default;
    AXTextRunLineID(void* containingBlock, size_t lineIndex)
        : containingBlock(containingBlock)
        , lineIndex(lineIndex)
    { }
    bool operator==(const AXTextRunLineID&) const = default;
    operator bool() const { return containingBlock; }
    String debugDescription() const
    {
        TextStream stream;
        stream << "LineID " << containingBlock << " " << lineIndex;
        return stream.release();
    }
};

struct AXTextRun {
    // The line index of this run within the context of the containing RenderBlock of the main-thread AX object.
    size_t lineIndex;
    // This data structure stores the DOM offsets that form the text runs that are concatenated to create |text|.
    // DOM offsets are offsets into the raw text node contents, pre-whitespace-collapse, while the |text| we store
    // is the rendered-text, post-whitespace-collapse.
    //
    // These offsets allow us to convert an offset into |text| (a "rendered-text offset") into a DOM offset, and
    // vice versa. This is required when we need to create a VisiblePosition from this text run.
    //
    // For example, consider this text, where "_" is a space: "__Charlie__Delta"
    // This would result in two inline textboxes in layout:
    // "Charlie "
    // "Delta"
    // which we combine into |text|: "Charlie Delta"
    // This Vector would then have values: [[2, 10], [11, 16]]
    FixedVector<std::array<uint16_t, 2>> textRunDomOffsets;

    // An array the size of the run, where each value is the width/advance of each character in the run (in the direction
    // of the writing mode: horizontal or vertical).
    FixedVector<uint16_t> characterAdvances;

    float lineHeight;

    // The distance between the RenderText's position and the start of the text run (useful for things that are not left-aligned, like `text-align: center`).
    float distanceFromBoundsInDirection;

    // The index in the containing AXTextRuns string where this run begins.
    unsigned startIndex { 0 };
    // The exclusive index in the containing AXTextRuns string where this run ends.
    unsigned endIndex { 0 };

    AXTextRun(size_t lineIndex, unsigned startIndex, unsigned endIndexInclusive, Vector<std::array<uint16_t, 2>>&& domOffsets, Vector<uint16_t>&& characterAdvances, float lineHeight, float distanceFromBoundsInDirection)
        : lineIndex(lineIndex)
        , textRunDomOffsets(WTFMove(domOffsets))
        , characterAdvances(WTFMove(characterAdvances))
        , lineHeight(lineHeight)
        , distanceFromBoundsInDirection(distanceFromBoundsInDirection)
        , startIndex(startIndex)
        , endIndex(endIndexInclusive)
    {
        // Runs should have a non-zero length (i.e. endIndex should strictly be greater than startIndex).
        // This is important because several parts of AXTextMarker rely on this assumption.
        ASSERT(endIndex > startIndex);
    }

    const FixedVector<std::array<uint16_t, 2>>& domOffsets() const { return textRunDomOffsets; }
    const FixedVector<uint16_t>& advances() const { return characterAdvances; }

    unsigned length() const { return endIndex - startIndex; }
};

struct AXTextRuns {
    WTF_MAKE_TZONE_ALLOCATED(AXTextRuns);
public:
    // The text for all runs, concatenated into a single string.
    String text;
    // The containing block for the text runs. This is required because based on the structure
    // of the AX tree, text runs for different objects can have the same line index but different
    // containing blocks, meaning they are rendered on different lines.
    // Do not de-reference. Use for comparison purposes only.
    void* containingBlock { nullptr };
    FixedVector<AXTextRun> runs;
    bool containsOnlyASCII { true };

    AXTextRuns() = default;
    AXTextRuns(RenderBlock* containingBlock, Vector<AXTextRun>&& textRuns, String&& text, bool containsOnlyASCII = true)
        : text(WTFMove(text))
        , containingBlock(containingBlock)
        , runs(WTFMove(textRuns))
        , containsOnlyASCII(containsOnlyASCII)
    { }

    String debugDescription() const;

    size_t size() const { return runs.size(); }
    const AXTextRun& at(size_t index) const { return (*this)[index]; }
    const AXTextRun& operator[](size_t index) const { return runs[index]; }

    unsigned runLength(size_t index) const { return runs[index].length(); }
    size_t lastRunIndex() const { return size() - 1; }
    unsigned lastRunLength() const
    {
        if (runs.isEmpty())
            return 0;
        return runLength(runs.size() - 1);
    }
    unsigned totalLength() const
    {
        unsigned size = runs.size();
        return size ? runLengthSumTo(size - 1) : 0;
    }
    unsigned runLengthSumTo(size_t index) const;
    unsigned domOffset(unsigned) const;

    size_t indexForOffset(unsigned textOffset, Affinity) const;
    AXTextRunLineID lineID(size_t index) const { return { containingBlock, runs[index].lineIndex }; }
    String toString() const { return text; }
    StringView toStringView() const { return StringView(text); }
    StringView substring(unsigned start, unsigned length = StringImpl::MaxLength) const
    {
        return StringView(text).substring(start, length);
    }

    String runString(size_t runIndex) const { return runStringView(runIndex).toString(); }
    StringView runStringView(size_t runIndex) const
    {
        const auto& run = runs[runIndex];
        return StringView(text).substring(run.startIndex, /* substringLength */ run.endIndex - run.startIndex);
    }

    // Returns a "local" rect representing the range specified by |start| and |end|.
    // "Local" means the rect is relative only to the top-left of this AXTextRuns instance.
    // For example, consider these runs where "|" represents |start| and |end|:
    //   aaaa
    //   b|bb|b
    // The local rect would be:
    //   {x: width_of_single_b, y: |lineHeight| * 1, width: width_of_two_b, height: |lineHeight * 1|}
    FloatRect localRect(unsigned start, unsigned end, FontOrientation) const;

    // Convenience methods for TextUnit movement.
    bool runStartsWithLineBreak(size_t runIndex) const { return text[runs[runIndex].startIndex] == '\n'; }
    bool runEndsWithLineBreak(size_t runIndex) const { return text[runs[runIndex].endIndex - 1] == '\n'; }
};

} // namespace WebCore
#endif // ENABLE(AX_THREAD_TEXT_APIS)
