/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(TEXT_AUTOSIZING)

#include "TextAutosizer.h"

#include "Document.h"
#include "InspectorInstrumentation.h"
#include "RenderObject.h"
#include "RenderText.h"
#include "RenderView.h"
#include "Settings.h"

namespace WebCore {

TextAutosizer::TextAutosizer(Document* document)
    : m_document(document)
{
}

TextAutosizer::~TextAutosizer()
{
}

bool TextAutosizer::processSubtree(RenderObject* layoutRoot)
{
    // FIXME: Text Autosizing should only be enabled when m_document->page()->mainFrame()->view()->useFixedLayout()
    // is true, but for now it's useful to ignore this so that it can be tested on desktop.
    if (!m_document->settings() || !m_document->settings()->textAutosizingEnabled() || layoutRoot->view()->printing() || !m_document->page())
        return false;

    IntSize windowSize = m_document->settings()->textAutosizingWindowSizeOverride();
    if (windowSize.isEmpty()) {
        Frame* mainFrame = m_document->page()->mainFrame();
        bool includeScrollbars = !InspectorInstrumentation::shouldApplyScreenWidthOverride(mainFrame);
        windowSize = mainFrame->view()->visibleContentRect(includeScrollbars).size(); // FIXME: Check that this is always in logical (density-independent) pixels (see wkbug.com/87440).
    }

    for (RenderObject* descendant = traverseNext(layoutRoot, layoutRoot); descendant; descendant = traverseNext(descendant, layoutRoot)) {
        if (!treatAsInline(descendant))
            processBlock(toRenderBlock(descendant), windowSize);
    }

    return true;
}

void TextAutosizer::processBlock(RenderBlock* block, const IntSize& windowSize)
{
    int windowLogicalWidth = block->isHorizontalWritingMode() ? windowSize.width() : windowSize.height();
    float multiplier = static_cast<float>(block->logicalWidth()) / windowLogicalWidth; // FIXME: This is overly simplistic.
    multiplier *= m_document->settings()->textAutosizingFontScaleFactor();

    if (multiplier < 1)
        return;
    for (RenderObject* descendant = traverseNext(block, block, treatAsInline); descendant; descendant = traverseNext(descendant, block, treatAsInline)) {
        if (descendant->isText())
            processText(toRenderText(descendant), multiplier);
    }
}

void TextAutosizer::processText(RenderText* text, float multiplier)
{
    float specifiedSize = text->style()->fontDescription().specifiedSize();
    float newSize = specifiedSize * multiplier; // FIXME: This is overly simplistic.

    RefPtr<RenderStyle> style = RenderStyle::clone(text->style());
    FontDescription fontDescription(style->fontDescription());
    fontDescription.setComputedSize(newSize);
    style->setFontDescription(fontDescription);
    style->font().update(style->font().fontSelector());
    text->setStyle(style.release());

    // FIXME: Increase computed line height proportionately.
    // FIXME: Increase list marker size proportionately.
}

bool TextAutosizer::treatAsInline(const RenderObject* renderer)
{
    return !renderer->isRenderBlock() || renderer->isListItem() || renderer->isInlineBlockOrInlineTable();
}

// FIXME: Consider making this a method on RenderObject if it remains this generic.
RenderObject* TextAutosizer::traverseNext(RenderObject* current, const RenderObject* stayWithin, RenderObjectFilter filter)
{
    for (RenderObject* child = current->firstChild(); child; child = child->nextSibling()) {
        if (!filter || filter(child)) {
            ASSERT(!stayWithin || child->isDescendantOf(stayWithin));
            return child;
        }
    }

    for (RenderObject* ancestor = current; ancestor; ancestor = ancestor->parent()) {
        if (ancestor == stayWithin)
            return 0;
        for (RenderObject* sibling = ancestor->nextSibling(); sibling; sibling = sibling->nextSibling()) {
            if (!filter || filter(sibling)) {
                ASSERT(!stayWithin || sibling->isDescendantOf(stayWithin));
                return sibling;
            }
        }
    }

    return 0;
}

} // namespace WebCore

#endif // ENABLE(TEXT_AUTOSIZING)
