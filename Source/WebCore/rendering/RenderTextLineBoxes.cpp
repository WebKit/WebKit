/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "RenderTextLineBoxes.h"

#include "InlineTextBox.h"
#include "RenderStyle.h"
#include "RootInlineBox.h"

namespace WebCore {

RenderTextLineBoxes::RenderTextLineBoxes()
    : m_first(nullptr)
    , m_last(nullptr)
{
}

InlineTextBox* RenderTextLineBoxes::createAndAppendLineBox(RenderText& renderText)
{
    auto textBox = renderText.createTextBox();
    if (!m_first) {
        m_first = textBox;
        m_last = textBox;
    } else {
        m_last->setNextTextBox(textBox);
        textBox->setPreviousTextBox(m_last);
        m_last = textBox;
    }
    textBox->setBehavesLikeText(true);
    return textBox;
}

void RenderTextLineBoxes::extract(InlineTextBox& box)
{
    checkConsistency();

    m_last = box.prevTextBox();
    if (&box == m_first)
        m_first = nullptr;
    if (box.prevTextBox())
        box.prevTextBox()->setNextTextBox(nullptr);
    box.setPreviousTextBox(nullptr);
    for (auto current = &box; current; current = current->nextTextBox())
        current->setExtracted();

    checkConsistency();
}

void RenderTextLineBoxes::attach(InlineTextBox& box)
{
    checkConsistency();

    if (m_last) {
        m_last->setNextTextBox(&box);
        box.setPreviousTextBox(m_last);
    } else
        m_first = &box;
    InlineTextBox* last = nullptr;
    for (auto current = &box; current; current = current->nextTextBox()) {
        current->setExtracted(false);
        last = current;
    }
    m_last = last;

    checkConsistency();
}

void RenderTextLineBoxes::remove(InlineTextBox& box)
{
    checkConsistency();

    if (&box == m_first)
        m_first = box.nextTextBox();
    if (&box == m_last)
        m_last = box.prevTextBox();
    if (box.nextTextBox())
        box.nextTextBox()->setPreviousTextBox(box.prevTextBox());
    if (box.prevTextBox())
        box.prevTextBox()->setNextTextBox(box.nextTextBox());

    checkConsistency();
}

void RenderTextLineBoxes::removeAllFromParent(RenderText& renderer)
{
    if (!m_first) {
        if (renderer.parent())
            renderer.parent()->dirtyLinesFromChangedChild(&renderer);
        return;
    }
    for (auto box = m_first; box; box = box->nextTextBox())
        box->remove();
}

void RenderTextLineBoxes::deleteAll(RenderText& renderer)
{
    if (!m_first)
        return;
    auto& arena = renderer.renderArena();
    InlineTextBox* next;
    for (auto current = m_first; current; current = next) {
        next = current->nextTextBox();
        current->destroy(arena);
    }
    m_first = nullptr;
    m_last = nullptr;
}

InlineTextBox* RenderTextLineBoxes::findNext(int offset, int& position) const
{
    if (!m_first)
        return nullptr;
    // FIXME: This looks buggy. The function is only used for debugging purposes.
    auto current = m_first;
    int currentOffset = current->len();
    while (offset > currentOffset && current->nextTextBox()) {
        current = current->nextTextBox();
        currentOffset = current->start() + current->len();
    }
    // we are now in the correct text run
    position = (offset > currentOffset ? current->len() : current->len() - (currentOffset - offset));
    return current;
}

IntRect RenderTextLineBoxes::boundingBox(const RenderText& renderer) const
{
    if (!m_first)
        return IntRect();

    // Return the width of the minimal left side and the maximal right side.
    float logicalLeftSide = 0;
    float logicalRightSide = 0;
    for (auto current = m_first; current; current = current->nextTextBox()) {
        if (current == m_first || current->logicalLeft() < logicalLeftSide)
            logicalLeftSide = current->logicalLeft();
        if (current == m_first || current->logicalRight() > logicalRightSide)
            logicalRightSide = current->logicalRight();
    }
    
    bool isHorizontal = renderer.style()->isHorizontalWritingMode();
    
    float x = isHorizontal ? logicalLeftSide : m_first->x();
    float y = isHorizontal ? m_first->y() : logicalLeftSide;
    float width = isHorizontal ? logicalRightSide - logicalLeftSide : m_last->logicalBottom() - x;
    float height = isHorizontal ? m_last->logicalBottom() - y : logicalRightSide - logicalLeftSide;
    return enclosingIntRect(FloatRect(x, y, width, height));
}

LayoutRect RenderTextLineBoxes::visualOverflowBoundingBox(const RenderText& renderer) const
{
    if (!m_first)
        return LayoutRect();

    // Return the width of the minimal left side and the maximal right side.
    auto logicalLeftSide = LayoutUnit::max();
    auto logicalRightSide = LayoutUnit::min();
    for (auto current = m_first; current; current = current->nextTextBox()) {
        logicalLeftSide = std::min(logicalLeftSide, current->logicalLeftVisualOverflow());
        logicalRightSide = std::max(logicalRightSide, current->logicalRightVisualOverflow());
    }
    
    auto logicalTop = m_first->logicalTopVisualOverflow();
    auto logicalWidth = logicalRightSide - logicalLeftSide;
    auto logicalHeight = m_last->logicalBottomVisualOverflow() - logicalTop;
    
    LayoutRect rect(logicalLeftSide, logicalTop, logicalWidth, logicalHeight);
    if (!renderer.style()->isHorizontalWritingMode())
        rect = rect.transposedRect();
    return rect;
}

bool RenderTextLineBoxes::hasRenderedText() const
{
    for (auto box = m_first; box; box = box->nextTextBox()) {
        if (box->len())
            return true;
    }
    return false;
}

int RenderTextLineBoxes::caretMinOffset() const
{
    auto box = m_first;
    if (!box)
        return 0;
    int minOffset = box->start();
    for (box = box->nextTextBox(); box; box = box->nextTextBox())
        minOffset = std::min<int>(minOffset, box->start());
    return minOffset;
}

int RenderTextLineBoxes::caretMaxOffset(const RenderText& renderer) const
{
    auto box = m_last;
    if (!box)
        return renderer.textLength();

    int maxOffset = box->start() + box->len();
    for (box = box->prevTextBox(); box; box = box->prevTextBox())
        maxOffset = std::max<int>(maxOffset, box->start() + box->len());
    return maxOffset;
}

void RenderTextLineBoxes::dirtyAll()
{
    for (auto box = m_first; box; box = box->nextTextBox())
        box->dirtyLineBoxes();
}

bool RenderTextLineBoxes::dirtyRange(RenderText& renderer, unsigned start, unsigned end, int lengthDelta)
{
    RootInlineBox* firstRootBox = nullptr;
    RootInlineBox* lastRootBox = nullptr;

    // Dirty all text boxes that include characters in between offset and offset+len.
    bool dirtiedLines = false;
    for (auto current = m_first; current; current = current->nextTextBox()) {
        // FIXME: This shouldn't rely on the end of a dirty line box. See https://bugs.webkit.org/show_bug.cgi?id=97264
        // Text run is entirely before the affected range.
        if (current->end() < start)
            continue;
        // Text run is entirely after the affected range.
        if (current->start() > end) {
            current->offsetRun(lengthDelta);
            auto& rootBox = current->root();
            if (!firstRootBox) {
                firstRootBox = &rootBox;
                if (!dirtiedLines) {
                    // The affected area was in between two runs. Go ahead and mark the root box of
                    // the run after the affected area as dirty.
                    firstRootBox->markDirty();
                    dirtiedLines = true;
                }
            }
            lastRootBox = &rootBox;
            continue;
        }
        if (current->end() >= start && current->end() <= end) {
            // Text run overlaps with the left end of the affected range.
            current->dirtyLineBoxes();
            dirtiedLines = true;
            continue;
        }
        if (current->start() <= start && current->end() >= end) {
            // Text run subsumes the affected range.
            current->dirtyLineBoxes();
            dirtiedLines = true;
            continue;
        }
        if (current->start() <= end && current->end() >= end) {
            // Text run overlaps with right end of the affected range.
            current->dirtyLineBoxes();
            dirtiedLines = true;
            continue;
        }
    }

    // Now we have to walk all of the clean lines and adjust their cached line break information
    // to reflect our updated offsets.
    if (lastRootBox)
        lastRootBox = lastRootBox->nextRootBox();
    if (firstRootBox) {
        auto previousRootBox = firstRootBox->prevRootBox();
        if (previousRootBox)
            firstRootBox = previousRootBox;
    } else if (m_last) {
        ASSERT(!lastRootBox);
        firstRootBox = &m_last->root();
        firstRootBox->markDirty();
        dirtiedLines = true;
    }
    for (auto current = firstRootBox; current && current != lastRootBox; current = current->nextRootBox()) {
        if (current->lineBreakObj() == &renderer && current->lineBreakPos() > end)
            current->setLineBreakPos(current->lineBreakPos() + lengthDelta);
    }
    
    // If the text node is empty, dirty the line where new text will be inserted.
    if (!m_first && renderer.parent()) {
        renderer.parent()->dirtyLinesFromChangedChild(&renderer);
        dirtiedLines = true;
    }
    return dirtiedLines;
}

#if !ASSERT_DISABLED
RenderTextLineBoxes::~RenderTextLineBoxes()
{
    ASSERT(!m_first);
    ASSERT(!m_last);
}

void RenderTextLineBoxes::checkConsistency() const
{
#ifdef CHECK_CONSISTENCY
    const InlineTextBox* prev = nullptr;
    for (auto child = m_first; child; child = child->nextTextBox()) {
        ASSERT(child->renderer() == this);
        ASSERT(child->prevTextBox() == prev);
        prev = child;
    }
    ASSERT(prev == m_last);
#endif
}
#endif

}
