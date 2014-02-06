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

#include "FileList.h"
#include "Font.h"
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

#if PLATFORM(IOS)
#include "StringTruncator.h"
#endif

namespace WebCore {

using namespace HTMLNames;

const int afterButtonSpacing = 4;
#if !PLATFORM(IOS)
const int iconHeight = 16;
const int iconWidth = 16;
const int iconFilenameSpacing = 2;
const int defaultWidthNumChars = 34;
#else
// On iOS the icon height matches the button height, to maximize the icon size.
const int iconFilenameSpacing = afterButtonSpacing;
const int defaultWidthNumChars = 38;
#endif
const int buttonShadowHeight = 2;

RenderFileUploadControl::RenderFileUploadControl(HTMLInputElement& input, PassRef<RenderStyle> style)
    : RenderBlockFlow(input, std::move(style))
    , m_canReceiveDroppedFiles(input.canReceiveDroppedFiles())
{
}

RenderFileUploadControl::~RenderFileUploadControl()
{
}

HTMLInputElement& RenderFileUploadControl::inputElement() const
{
    return toHTMLInputElement(nodeForNonAnonymous());
}

void RenderFileUploadControl::updateFromElement()
{
    ASSERT(inputElement().isFileUpload());

    if (HTMLInputElement* button = uploadButton()) {
        bool newCanReceiveDroppedFilesState = inputElement().canReceiveDroppedFiles();
        if (m_canReceiveDroppedFiles != newCanReceiveDroppedFilesState) {
            m_canReceiveDroppedFiles = newCanReceiveDroppedFilesState;
            button->setActive(newCanReceiveDroppedFilesState);
        }
    }

    // This only supports clearing out the files, but that's OK because for
    // security reasons that's the only change the DOM is allowed to make.
    FileList* files = inputElement().files();
    ASSERT(files);
    if (files && files->isEmpty())
        repaint();
}

static int nodeWidth(Node* node)
{
    return (node && node->renderBox()) ? node->renderBox()->pixelSnappedWidth() : 0;
}

#if PLATFORM(IOS)
static int nodeHeight(Node* node)
{
    return (node && node->renderBox()) ? node->renderBox()->pixelSnappedHeight() : 0;
}
#endif

int RenderFileUploadControl::maxFilenameWidth() const
{
#if PLATFORM(IOS)
    int iconWidth = nodeHeight(uploadButton());
#endif
    return std::max(0, contentBoxRect().pixelSnappedWidth() - nodeWidth(uploadButton()) - afterButtonSpacing
        - (inputElement().icon() ? iconWidth + iconFilenameSpacing : 0));
}

void RenderFileUploadControl::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (style().visibility() != VISIBLE)
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
        const Font& font = style().font();
        TextRun textRun = constructTextRun(this, font, displayedFilename, style(), TextRun::AllowTrailingExpansion, RespectDirection | RespectDirectionOverride);
        textRun.disableRoundingHacks();

#if PLATFORM(IOS)
        int iconHeight = nodeHeight(uploadButton());
        int iconWidth = iconHeight;
#endif
        // Determine where the filename should be placed
        LayoutUnit contentLeft = paintOffset.x() + borderLeft() + paddingLeft();
        HTMLInputElement* button = uploadButton();
        if (!button)
            return;

        LayoutUnit buttonWidth = nodeWidth(button);
        LayoutUnit buttonAndIconWidth = buttonWidth + afterButtonSpacing
            + (inputElement().icon() ? iconWidth + iconFilenameSpacing : 0);
        LayoutUnit textX;
        if (style().isLeftToRightDirection())
            textX = contentLeft + buttonAndIconWidth;
        else
            textX = contentLeft + contentWidth() - buttonAndIconWidth - font.width(textRun);

        LayoutUnit textY = 0;
        // We want to match the button's baseline
        // FIXME: Make this work with transforms.
        if (RenderButton* buttonRenderer = toRenderButton(button->renderer()))
            textY = paintOffset.y() + borderTop() + paddingTop() + buttonRenderer->baselinePosition(AlphabeticBaseline, true, HorizontalLine, PositionOnContainingLine);
        else
            textY = baselinePosition(AlphabeticBaseline, true, HorizontalLine, PositionOnContainingLine);

        paintInfo.context->setFillColor(style().visitedDependentColor(CSSPropertyColor), style().colorSpace());
        
        // Draw the filename
        paintInfo.context->drawBidiText(font, textRun, IntPoint(roundToInt(textX), roundToInt(textY)));
        
        if (inputElement().icon()) {
            // Determine where the icon should be placed
            LayoutUnit iconY = paintOffset.y() + borderTop() + paddingTop() + (contentHeight() - iconHeight) / 2;
            LayoutUnit iconX;
            if (style().isLeftToRightDirection())
                iconX = contentLeft + buttonWidth + afterButtonSpacing;
            else
                iconX = contentLeft + contentWidth() - buttonWidth - afterButtonSpacing - iconWidth;

#if PLATFORM(IOS)
            if (RenderButton* buttonRenderer = toRenderButton(button->renderer())) {
                // Draw the file icon and decorations.
                IntRect iconRect(iconX, iconY, iconWidth, iconHeight);
                RenderTheme::FileUploadDecorations decorationsType = inputElement().files()->length() == 1 ? RenderTheme::SingleFile : RenderTheme::MultipleFiles;
                theme().paintFileUploadIconDecorations(this, buttonRenderer, paintInfo, iconRect, inputElement().icon(), decorationsType);
            }
#else
            // Draw the file icon
            inputElement().icon()->paint(paintInfo.context, IntRect(roundToInt(iconX), roundToInt(iconY), iconWidth, iconHeight));
#endif
        }
    }

    // Paint the children.
    RenderBlockFlow::paintObject(paintInfo, paintOffset);
}

void RenderFileUploadControl::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    // Figure out how big the filename space needs to be for a given number of characters
    // (using "0" as the nominal character).
    const UChar character = '0';
    const String characterAsString = String(&character, 1);
    const Font& font = style().font();
    // FIXME: Remove the need for this const_cast by making constructTextRun take a const RenderObject*.
    RenderFileUploadControl* renderer = const_cast<RenderFileUploadControl*>(this);
    float minDefaultLabelWidth = defaultWidthNumChars * font.width(constructTextRun(renderer, font, characterAsString, style(), TextRun::AllowTrailingExpansion));

    const String label = theme().fileListDefaultLabel(inputElement().multiple());
    float defaultLabelWidth = font.width(constructTextRun(renderer, font, label, style(), TextRun::AllowTrailingExpansion));
    if (HTMLInputElement* button = uploadButton())
        if (RenderObject* buttonRenderer = button->renderer())
            defaultLabelWidth += buttonRenderer->maxPreferredLogicalWidth() + afterButtonSpacing;
    maxLogicalWidth = static_cast<int>(ceilf(std::max(minDefaultLabelWidth, defaultLabelWidth)));

    if (!style().width().isPercent())
        minLogicalWidth = maxLogicalWidth;
}

void RenderFileUploadControl::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (style().width().isFixed() && style().width().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(style().width().value());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    if (style().minWidth().isFixed() && style().minWidth().value() > 0) {
        m_maxPreferredLogicalWidth = std::max(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(style().minWidth().value()));
        m_minPreferredLogicalWidth = std::max(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(style().minWidth().value()));
    }

    if (style().maxWidth().isFixed()) {
        m_maxPreferredLogicalWidth = std::min(m_maxPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(style().maxWidth().value()));
        m_minPreferredLogicalWidth = std::min(m_minPreferredLogicalWidth, adjustContentBoxLogicalWidthForBoxSizing(style().maxWidth().value()));
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
    ASSERT(inputElement().shadowRoot());
    Node* buttonNode = inputElement().shadowRoot()->firstChild();
    return buttonNode && buttonNode->isHTMLElement() && isHTMLInputElement(buttonNode) ? toHTMLInputElement(buttonNode) : 0;
}

String RenderFileUploadControl::buttonValue()
{
    if (HTMLInputElement* button = uploadButton())
        return button->value();
    
    return String();
}

String RenderFileUploadControl::fileTextValue() const
{
    ASSERT(inputElement().files());
#if PLATFORM(IOS)
    if (inputElement().files()->length())
        return StringTruncator::rightTruncate(inputElement().displayString(), maxFilenameWidth(), style().font(), StringTruncator::EnableRoundingHacks);
#endif
    return theme().fileListNameForWidth(inputElement().files(), style().font(), maxFilenameWidth(), inputElement().multiple());
}
    
} // namespace WebCore
