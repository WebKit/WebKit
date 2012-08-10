/*
 * Copyright (C) 2006, 2007, 2012 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "RenderFileUploadControl.h"

#include "ElementShadow.h"
#include "FileList.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Icon.h"
#include "LocalizedStrings.h"
#include "PaintInfo.h"
#include "RenderButton.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "ShadowRoot.h"
#include "TextRun.h"
#include "VisiblePosition.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

const int afterButtonSpacing = 4;
const int iconHeight = 16;
const int iconWidth = 16;
const int iconFilenameSpacing = 2;
const int defaultWidthNumChars = 34;
const int buttonShadowHeight = 2;

RenderFileUploadControl::RenderFileUploadControl(HTMLInputElement* input)
    : RenderBlock(input)
    , m_canReceiveDroppedFiles(input->canReceiveDroppedFiles())
{
}

RenderFileUploadControl::~RenderFileUploadControl()
{
}

bool RenderFileUploadControl::canBeReplacedWithInlineRunIn() const
{
    return false;
}

void RenderFileUploadControl::updateFromElement()
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    ASSERT(input->isFileUpload());

    if (HTMLInputElement* button = uploadButton()) {
        bool newDisabled = !theme()->isEnabled(this);
        // We should avoid to call HTMLFormControlElement::setDisabled() as
        // possible because setAttribute() in setDisabled() can cause style
        // recalculation, and HTMLFormControlElement::recalcStyle() calls
        // updateFromElement() eventually.
        if (button->disabled() != newDisabled)
            button->setDisabled(newDisabled);

        bool newCanReceiveDroppedFilesState = input->canReceiveDroppedFiles();
        if (m_canReceiveDroppedFiles != newCanReceiveDroppedFilesState) {
            m_canReceiveDroppedFiles = newCanReceiveDroppedFilesState;
            button->setActive(newCanReceiveDroppedFilesState);
        }
    }

    // This only supports clearing out the files, but that's OK because for
    // security reasons that's the only change the DOM is allowed to make.
    FileList* files = input->files();
    ASSERT(files);
    if (files && files->isEmpty())
        repaint();
}

static int nodeWidth(Node* node)
{
    return node ? node->renderBox()->pixelSnappedWidth() : 0;
}

int RenderFileUploadControl::maxFilenameWidth() const
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    return max(0, contentBoxRect().pixelSnappedWidth() - nodeWidth(uploadButton()) - afterButtonSpacing
        - (input->icon() ? iconWidth + iconFilenameSpacing : 0));
}

void RenderFileUploadControl::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (style()->visibility() != VISIBLE)
        return;
    
    // Push a clip.
    GraphicsContextStateSaver stateSaver(*paintInfo.context, false);
    if (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseChildBlockBackgrounds) {
        IntRect clipRect = enclosingIntRect(LayoutRect(paintOffset.x() + borderLeft(), paintOffset.y() + borderTop(),
                         width() - borderLeft() - borderRight(), height() - borderBottom() - borderTop() + buttonShadowHeight));
        if (clipRect.isEmpty())
            return;
        stateSaver.save();
        paintInfo.context->clip(clipRect);
    }

    if (paintInfo.phase == PaintPhaseForeground) {
        const String& displayedFilename = fileTextValue();
        const Font& font = style()->font();
        TextRun textRun = constructTextRun(this, font, displayedFilename, style(), TextRun::AllowTrailingExpansion, RespectDirection | RespectDirectionOverride);
        textRun.disableRoundingHacks();

        // Determine where the filename should be placed
        LayoutUnit contentLeft = paintOffset.x() + borderLeft() + paddingLeft();
        HTMLInputElement* button = uploadButton();
        if (!button)
            return;

        HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
        LayoutUnit buttonWidth = nodeWidth(button);
        LayoutUnit buttonAndIconWidth = buttonWidth + afterButtonSpacing
            + (input->icon() ? iconWidth + iconFilenameSpacing : 0);
        LayoutUnit textX;
        if (style()->isLeftToRightDirection())
            textX = contentLeft + buttonAndIconWidth;
        else
            textX = contentLeft + contentWidth() - buttonAndIconWidth - font.width(textRun);
        // We want to match the button's baseline
        RenderButton* buttonRenderer = toRenderButton(button->renderer());
        // FIXME: Make this work with transforms.
        LayoutUnit textY = paintOffset.y() + buttonRenderer->baselinePosition(AlphabeticBaseline, true, HorizontalLine, PositionOnContainingLine);

        paintInfo.context->setFillColor(style()->visitedDependentColor(CSSPropertyColor), style()->colorSpace());
        
        // Draw the filename
        paintInfo.context->drawBidiText(font, textRun, IntPoint(roundToInt(textX), roundToInt(textY)));
        
        if (input->icon()) {
            // Determine where the icon should be placed
            LayoutUnit iconY = paintOffset.y() + borderTop() + paddingTop() + (contentHeight() - iconHeight) / 2;
            LayoutUnit iconX;
            if (style()->isLeftToRightDirection())
                iconX = contentLeft + buttonWidth + afterButtonSpacing;
            else
                iconX = contentLeft + contentWidth() - buttonWidth - afterButtonSpacing - iconWidth;

            // Draw the file icon
            input->icon()->paint(paintInfo.context, IntRect(roundToInt(iconX), roundToInt(iconY), iconWidth, iconHeight));
        }
    }

    // Paint the children.
    RenderBlock::paintObject(paintInfo, paintOffset);
}

void RenderFileUploadControl::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    RenderStyle* style = this->style();
    ASSERT(style);

    const Font& font = style->font();
    if (style->width().isFixed() && style->width().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = computeContentBoxLogicalWidth(style->width().value());
    else {
        // Figure out how big the filename space needs to be for a given number of characters
        // (using "0" as the nominal character).
        const UChar character = '0';
        const String characterAsString = String(&character, 1);
        float minDefaultLabelWidth = defaultWidthNumChars * font.width(constructTextRun(this, font, characterAsString, style, TextRun::AllowTrailingExpansion));

        const String label = theme()->fileListDefaultLabel(node()->toInputElement()->multiple());
        float defaultLabelWidth = font.width(constructTextRun(this, font, label, style, TextRun::AllowTrailingExpansion));
        if (HTMLInputElement* button = uploadButton())
            if (RenderObject* buttonRenderer = button->renderer())
                defaultLabelWidth += buttonRenderer->maxPreferredLogicalWidth() + afterButtonSpacing;
        m_maxPreferredLogicalWidth = static_cast<int>(ceilf(max(minDefaultLabelWidth, defaultLabelWidth)));
    }

    if (style->minWidth().isFixed() && style->minWidth().value() > 0) {
        m_maxPreferredLogicalWidth = max(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(style->minWidth().value()));
        m_minPreferredLogicalWidth = max(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(style->minWidth().value()));
    } else if (style->width().isPercent() || (style->width().isAuto() && style->height().isPercent()))
        m_minPreferredLogicalWidth = 0;
    else
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth;

    if (style->maxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = min(m_maxPreferredLogicalWidth, computeContentBoxLogicalWidth(style->maxWidth().value()));
        m_minPreferredLogicalWidth = min(m_minPreferredLogicalWidth, computeContentBoxLogicalWidth(style->maxWidth().value()));
    }

    int toAdd = borderAndPaddingWidth();
    m_minPreferredLogicalWidth += toAdd;
    m_maxPreferredLogicalWidth += toAdd;

    setPreferredLogicalWidthsDirty(false);
}

VisiblePosition RenderFileUploadControl::positionForPoint(const LayoutPoint&)
{
    return VisiblePosition();
}

HTMLInputElement* RenderFileUploadControl::uploadButton() const
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());

    ASSERT(input->shadow());

    Node* buttonNode = input->shadow()->oldestShadowRoot()->firstChild();
    return buttonNode && buttonNode->isHTMLElement() && buttonNode->hasTagName(inputTag) ? static_cast<HTMLInputElement*>(buttonNode) : 0;
}

String RenderFileUploadControl::buttonValue()
{
    if (HTMLInputElement* button = uploadButton())
        return button->value();
    
    return String();
}

String RenderFileUploadControl::fileTextValue() const
{
    HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
    ASSERT(input->files());
    return theme()->fileListNameForWidth(input->files(), style()->font(), maxFilenameWidth(), input->multiple());
}
    
} // namespace WebCore
