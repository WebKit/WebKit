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
#include "FontCascade.h"
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
#include "StringTruncator.h"
#include "TextRun.h"
#include "VisiblePosition.h"
#include <math.h>
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderFileUploadControl);

const int afterButtonSpacing = 4;
#if !PLATFORM(IOS_FAMILY)
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

RenderFileUploadControl::RenderFileUploadControl(HTMLInputElement& input, RenderStyle&& style)
    : RenderBlockFlow(input, WTFMove(style))
    , m_canReceiveDroppedFiles(input.canReceiveDroppedFiles())
{
}

RenderFileUploadControl::~RenderFileUploadControl() = default;

HTMLInputElement& RenderFileUploadControl::inputElement() const
{
    return downcast<HTMLInputElement>(nodeForNonAnonymous());
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
    return (node && node->renderBox()) ? roundToInt(node->renderBox()->size().width()) : 0;
}

#if PLATFORM(IOS_FAMILY)
static int nodeHeight(Node* node)
{
    return (node && node->renderBox()) ? roundToInt(node->renderBox()->size().height()) : 0;
}
#endif

int RenderFileUploadControl::maxFilenameWidth() const
{
#if PLATFORM(IOS_FAMILY)
    int iconWidth = nodeHeight(uploadButton());
#endif
    return std::max(0, snappedIntRect(contentBoxRect()).width() - nodeWidth(uploadButton()) - afterButtonSpacing
        - (inputElement().icon() ? iconWidth + iconFilenameSpacing : 0));
}

void RenderFileUploadControl::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (style().visibility() != Visibility::Visible)
        return;
    
    // Push a clip.
    GraphicsContextStateSaver stateSaver(paintInfo.context(), false);
    if (paintInfo.phase == PaintPhase::Foreground || paintInfo.phase == PaintPhase::ChildBlockBackgrounds) {
        IntRect clipRect = enclosingIntRect(LayoutRect(paintOffset.x() + borderLeft(), paintOffset.y() + borderTop(),
                         width() - borderLeft() - borderRight(), height() - borderBottom() - borderTop() + buttonShadowHeight));
        if (clipRect.isEmpty())
            return;
        stateSaver.save();
        paintInfo.context().clip(clipRect);
    }

    if (paintInfo.phase == PaintPhase::Foreground) {
        const String& displayedFilename = fileTextValue();
        const FontCascade& font = style().fontCascade();
        TextRun textRun = constructTextRun(displayedFilename, style(), AllowTrailingExpansion, RespectDirection | RespectDirectionOverride);

#if PLATFORM(IOS_FAMILY)
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

        LayoutUnit textY;
        // We want to match the button's baseline
        // FIXME: Make this work with transforms.
        if (RenderButton* buttonRenderer = downcast<RenderButton>(button->renderer()))
            textY = paintOffset.y() + borderTop() + paddingTop() + buttonRenderer->baselinePosition(AlphabeticBaseline, true, HorizontalLine, PositionOnContainingLine);
        else
            textY = baselinePosition(AlphabeticBaseline, true, HorizontalLine, PositionOnContainingLine);

        paintInfo.context().setFillColor(style().visitedDependentColorWithColorFilter(CSSPropertyColor));
        
        // Draw the filename
        paintInfo.context().drawBidiText(font, textRun, IntPoint(roundToInt(textX), roundToInt(textY)));
        
        if (inputElement().icon()) {
            // Determine where the icon should be placed
            LayoutUnit iconY = paintOffset.y() + borderTop() + paddingTop() + (contentHeight() - iconHeight) / 2;
            LayoutUnit iconX;
            if (style().isLeftToRightDirection())
                iconX = contentLeft + buttonWidth + afterButtonSpacing;
            else
                iconX = contentLeft + contentWidth() - buttonWidth - afterButtonSpacing - iconWidth;

#if PLATFORM(IOS_FAMILY)
            if (RenderButton* buttonRenderer = downcast<RenderButton>(button->renderer())) {
                // Draw the file icon and decorations.
                IntRect iconRect(iconX, iconY, iconWidth, iconHeight);
                RenderTheme::FileUploadDecorations decorationsType = inputElement().files()->length() == 1 ? RenderTheme::SingleFile : RenderTheme::MultipleFiles;
                theme().paintFileUploadIconDecorations(*this, *buttonRenderer, paintInfo, iconRect, inputElement().icon(), decorationsType);
            }
#else
            // Draw the file icon
            inputElement().icon()->paint(paintInfo.context(), IntRect(roundToInt(iconX), roundToInt(iconY), iconWidth, iconHeight));
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
    const FontCascade& font = style().fontCascade();
    // FIXME: Remove the need for this const_cast by making constructTextRun take a const RenderObject*.
    float minDefaultLabelWidth = defaultWidthNumChars * font.width(constructTextRun(characterAsString, style(), AllowTrailingExpansion));

    const String label = theme().fileListDefaultLabel(inputElement().multiple());
    float defaultLabelWidth = font.width(constructTextRun(label, style(), AllowTrailingExpansion));
    if (HTMLInputElement* button = uploadButton())
        if (RenderObject* buttonRenderer = button->renderer())
            defaultLabelWidth += buttonRenderer->maxPreferredLogicalWidth() + afterButtonSpacing;
    maxLogicalWidth = static_cast<int>(ceilf(std::max(minDefaultLabelWidth, defaultLabelWidth)));

    if (!style().width().isPercentOrCalculated())
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

    int toAdd = horizontalBorderAndPaddingExtent();
    m_minPreferredLogicalWidth += toAdd;
    m_maxPreferredLogicalWidth += toAdd;

    setPreferredLogicalWidthsDirty(false);
}

VisiblePosition RenderFileUploadControl::positionForPoint(const LayoutPoint&, const RenderFragmentContainer*)
{
    return VisiblePosition();
}

HTMLInputElement* RenderFileUploadControl::uploadButton() const
{
    ASSERT(inputElement().shadowRoot());
    Node* buttonNode = inputElement().shadowRoot()->firstChild();
    return is<HTMLInputElement>(buttonNode) ? downcast<HTMLInputElement>(buttonNode) : nullptr;
}

String RenderFileUploadControl::buttonValue()
{
    if (HTMLInputElement* button = uploadButton())
        return button->value();
    
    return String();
}

String RenderFileUploadControl::fileTextValue() const
{
    auto& input = inputElement();
    ASSERT(inputElement().files());
    if (input.files()->length() && !input.displayString().isEmpty())
        return StringTruncator::rightTruncate(input.displayString(), maxFilenameWidth(), style().fontCascade());
    return theme().fileListNameForWidth(input.files(), style().fontCascade(), maxFilenameWidth(), input.multiple());
}
    
} // namespace WebCore
