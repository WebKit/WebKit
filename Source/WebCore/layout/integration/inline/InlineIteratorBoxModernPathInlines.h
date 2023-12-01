/**
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#include "InlineDisplayBoxInlines.h"
#include "InlineIteratorBoxModernPath.h"
#include "LayoutBoxInlines.h"

namespace WebCore {
namespace InlineIterator {

inline bool BoxModernPath::isHorizontal() const { return box().isHorizontal(); }

inline TextRun BoxModernPath::textRun(TextRunMode mode) const
{
    auto& style = box().style();
    auto expansion = box().expansion();
    auto logicalLeft = [&] {
        if (style.isLeftToRightDirection())
            return visualRectIgnoringBlockDirection().x() - (line().lineBoxLeft() + line().contentLogicalLeft());
        return line().lineBoxRight() - (visualRectIgnoringBlockDirection().maxX() + line().contentLogicalLeft());
    };
    auto characterScanForCodePath = isText() && !renderText().canUseSimpleFontCodePath();
    auto textRun = TextRun { mode == TextRunMode::Editing ? originalText() : box().text().renderedContent(), logicalLeft(), expansion.horizontalExpansion, expansion.behavior, direction(), style.rtlOrdering() == Order::Visual, characterScanForCodePath };
    textRun.setTabSize(!style.collapseWhiteSpace(), style.tabSize());
    return textRun;
}

inline BoxModernPath BoxModernPath::parentInlineBox() const
{
    ASSERT(!atEnd());

    auto candidate = *this;

    if (isRootInlineBox()) {
        candidate.setAtEnd();
        return candidate;
    }

    auto& parentLayoutBox = box().layoutBox().parent();
    do {
        candidate.traversePreviousBox();
    } while (!candidate.atEnd() && &candidate.box().layoutBox() != &parentLayoutBox);

    ASSERT(candidate.atEnd() || candidate.box().isInlineBox());

    return candidate;
}

inline bool BoxModernPath::isWithinInlineBox(const Layout::Box& inlineBox)
{
    auto* layoutBox = &box().layoutBox().parent();
    for (; layoutBox->isInlineBox(); layoutBox = &layoutBox->parent()) {
        if (layoutBox == &inlineBox)
            return true;
    }
    return false;
}

inline BoxModernPath BoxModernPath::firstLeafBoxForInlineBox() const
{
    ASSERT(box().isInlineBox());

    auto& inlineBox = box().layoutBox();

    // The next box is the first descendant of this box;
    auto first = *this;
    first.traverseNextOnLine();

    if (!first.atEnd() && !first.isWithinInlineBox(inlineBox))
        first.setAtEnd();

    return first;
}

inline BoxModernPath BoxModernPath::lastLeafBoxForInlineBox() const
{
    ASSERT(box().isInlineBox());

    auto& inlineBox = box().layoutBox();

    // FIXME: Get the last box index directly from the display box.
    auto last = firstLeafBoxForInlineBox();
    for (auto box = last; !box.atEnd() && box.isWithinInlineBox(inlineBox); box.traverseNextOnLine())
        last = box;

    return last;
}

}
}
