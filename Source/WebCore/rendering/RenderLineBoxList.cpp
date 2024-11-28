/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "RenderLineBoxList.h"

#include "HitTestResult.h"
#include "LegacyInlineTextBox.h"
#include "LegacyRootInlineBox.h"
#include "PaintInfo.h"
#include "RenderBlockFlow.h"
#include "RenderInline.h"
#include "RenderLineBreak.h"
#include "RenderSVGInline.h"
#include "RenderStyleInlines.h"
#include "RenderView.h"

namespace WebCore {

#ifndef NDEBUG
RenderLineBoxList::~RenderLineBoxList()
{
    ASSERT(!m_firstLineBox);
    ASSERT(!m_lastLineBox);
}
#endif

void RenderLineBoxList::appendLineBox(std::unique_ptr<LegacyInlineFlowBox> box)
{
    checkConsistency();

    LegacyInlineFlowBox* boxPtr = box.release();

    if (!m_firstLineBox) {
        m_firstLineBox = boxPtr;
        m_lastLineBox = boxPtr;
    } else {
        m_lastLineBox->setNextLineBox(boxPtr);
        boxPtr->setPreviousLineBox(m_lastLineBox);
        m_lastLineBox = boxPtr;
    }

    checkConsistency();
}

void RenderLineBoxList::deleteLineBoxTree()
{
    LegacyInlineFlowBox* line = m_firstLineBox;
    LegacyInlineFlowBox* nextLine;
    while (line) {
        nextLine = line->nextLineBox();
        line->deleteLine();
        line = nextLine;
    }
    m_firstLineBox = m_lastLineBox = nullptr;
}

void RenderLineBoxList::extractLineBox(LegacyInlineFlowBox* box)
{
    checkConsistency();
    
    m_lastLineBox = box->prevLineBox();
    if (box == m_firstLineBox)
        m_firstLineBox = 0;
    if (box->prevLineBox())
        box->prevLineBox()->setNextLineBox(nullptr);
    box->setPreviousLineBox(nullptr);
    for (auto* curr = box; curr; curr = curr->nextLineBox())
        curr->setExtracted();

    checkConsistency();
}

void RenderLineBoxList::attachLineBox(LegacyInlineFlowBox* box)
{
    checkConsistency();

    if (m_lastLineBox) {
        m_lastLineBox->setNextLineBox(box);
        box->setPreviousLineBox(m_lastLineBox);
    } else
        m_firstLineBox = box;
    LegacyInlineFlowBox* last = box;
    for (auto* curr = box; curr; curr = curr->nextLineBox()) {
        curr->setExtracted(false);
        last = curr;
    }
    m_lastLineBox = last;

    checkConsistency();
}

void RenderLineBoxList::removeLineBox(LegacyInlineFlowBox* box)
{
    checkConsistency();

    if (box == m_firstLineBox)
        m_firstLineBox = box->nextLineBox();
    if (box == m_lastLineBox)
        m_lastLineBox = box->prevLineBox();
    if (box->nextLineBox())
        box->nextLineBox()->setPreviousLineBox(box->prevLineBox());
    if (box->prevLineBox())
        box->prevLineBox()->setNextLineBox(box->nextLineBox());

    checkConsistency();
}

void RenderLineBoxList::deleteLineBoxes()
{
    if (m_firstLineBox) {
        LegacyInlineFlowBox* next;
        for (auto* curr = m_firstLineBox; curr; curr = next) {
            next = curr->nextLineBox();
            delete curr;
        }
        m_firstLineBox = nullptr;
        m_lastLineBox = nullptr;
    }
}

void RenderLineBoxList::dirtyLineBoxes()
{
    for (auto* curr = firstLegacyLineBox(); curr; curr = curr->nextLineBox())
        curr->dirtyLineBoxes();
}

void RenderLineBoxList::shiftLinesBy(LayoutUnit shiftX, LayoutUnit shiftY)
{
    for (auto* box = firstLegacyLineBox(); box; box = box->nextLineBox())
        box->adjustPosition(shiftX, shiftY);
}

void RenderLineBoxList::dirtyLineFromChangedChild(RenderBoxModelObject& container)
{
    ASSERT(is<RenderInline>(container) || is<RenderBlockFlow>(container));
    if (!container.isSVGRenderer())
        return;

    if (!container.parent() || (is<RenderBlockFlow>(container) && container.selfNeedsLayout()))
        return;

    auto* inlineContainer = dynamicDowncast<RenderSVGInline>(container);
    if (auto* lineBox = inlineContainer ? inlineContainer->firstLegacyInlineBox() : firstLegacyLineBox()) {
        lineBox->root().markDirty();
        return;
    }
    // For an empty inline, propagate the check up to our parent.
    if (inlineContainer && inlineContainer->everHadLayout()) {
        auto* parent = inlineContainer->parent();
        parent->dirtyLineFromChangedChild();
        parent->setNeedsLayout();
    }
}

#ifndef NDEBUG

void RenderLineBoxList::checkConsistency() const
{
#ifdef CHECK_CONSISTENCY
    const LegacyInlineFlowBox* prev = nullptr;
    for (const LegacyInlineFlowBox* child = m_firstLineBox; child != nullptr; child = child->nextLineBox()) {
        ASSERT(child->prevLineBox() == prev);
        prev = child;
    }
    ASSERT(prev == m_lastLineBox);
#endif
}

#endif

}
