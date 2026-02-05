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

#include "ContainerNodeInlines.h"
#include "ElementRareData.h"
#include "FileList.h"
#include "FontCascade.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Icon.h"
#include "InlineIteratorInlineBox.h"
#include "LocalizedStrings.h"
#include "NodeInlines.h"
#include "PaintInfo.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderButton.h"
#include "RenderElementStyleInlines.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "ShadowRoot.h"
#include "StringTruncator.h"
#include "TextRun.h"
#include "VisiblePosition.h"
#include <math.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace HTMLNames;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderFileUploadControl);

constexpr int afterButtonSpacing = 4;
constexpr int buttonShadowHeight = 2;

#if !PLATFORM(COCOA)
// On Cocoa platforms the icon height matches the button height, to maximize the icon size.
constexpr int iconLogicalHeight = 16;
constexpr int iconLogicalWidth = 16;
#endif

#if !PLATFORM(IOS_FAMILY)
constexpr int iconFilenameSpacing = 2;
constexpr int defaultWidthNumChars = 34;
#else
constexpr int iconFilenameSpacing = afterButtonSpacing;
constexpr int defaultWidthNumChars = 38;
#endif

RenderFileUploadControl::RenderFileUploadControl(HTMLInputElement& input, RenderStyle&& style)
    : RenderBlockFlow(Type::FileUploadControl, input, WTF::move(style))
    , m_canReceiveDroppedFiles(input.canReceiveDroppedFiles())
{
    ASSERT(isRenderFileUploadControl());
}

RenderFileUploadControl::~RenderFileUploadControl() = default;

HTMLInputElement& RenderFileUploadControl::inputElement() const
{
    return downcast<HTMLInputElement>(nodeForNonAnonymous());
}

void RenderFileUploadControl::updateFromElement()
{
    ASSERT(inputElement().isFileUpload());

    if (RefPtr button = uploadButton()) {
        bool newCanReceiveDroppedFilesState = inputElement().canReceiveDroppedFiles();
        if (m_canReceiveDroppedFiles != newCanReceiveDroppedFilesState) {
            m_canReceiveDroppedFiles = newCanReceiveDroppedFilesState;
            button->setActive(newCanReceiveDroppedFilesState);
        }
    }

    // This only supports clearing out the files, but that's OK because for
    // security reasons that's the only change the DOM is allowed to make.
    RefPtr files = inputElement().files();
    ASSERT(files);
    if (files && files->isEmpty())
        repaint();
}

static int nodeLogicalWidth(Node* node)
{
    return (node && node->renderBox()) ? roundToInt(node->renderBox()->logicalSize().width()) : 0;
}

#if PLATFORM(COCOA)
static int nodeLogicalHeight(Node* node)
{
    return (node && node->renderBox()) ? roundToInt(node->renderBox()->logicalSize().height()) : 0;
}
#endif

int RenderFileUploadControl::maxFilenameLogicalWidth() const
{
#if PLATFORM(COCOA)
    int iconLogicalWidth = nodeLogicalHeight(uploadButton());
#endif
    return std::max(0, roundToInt(contentBoxLogicalWidth()) - nodeLogicalWidth(uploadButton()) - afterButtonSpacing
        - (inputElement().icon() ? iconLogicalWidth + iconFilenameSpacing : 0));
}

void RenderFileUploadControl::paintObject(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (style().usedVisibility() != Visibility::Visible)
        return;
    
    if (!paintInfo.context().paintingDisabled())
        paintControl(paintInfo, paintOffset);

    // Paint the children.
    RenderBlockFlow::paintObject(paintInfo, paintOffset);
}

void RenderFileUploadControl::paintControl(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
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

    auto isHorizontalWritingMode = writingMode().isHorizontal();
    auto isBlockFlipped = writingMode().isBlockFlipped();
    auto logicalPaintOffset = isHorizontalWritingMode ? paintOffset : paintOffset.transposedPoint();

    if (paintInfo.phase == PaintPhase::Foreground) {
        const String& displayedFilename = fileTextValue();
        const FontCascade& font = style().fontCascade();
        TextRun textRun = constructTextRun(displayedFilename, style(), ExpansionBehavior::allowRightOnly(), RespectDirection | RespectDirectionOverride);
#if PLATFORM(COCOA)
        int iconLogicalHeight = nodeLogicalHeight(uploadButton());
        int iconLogicalWidth = iconLogicalHeight;
#endif
        // Determine where the filename should be placed
        LayoutUnit contentLogicalLeft = logicalPaintOffset.x() + logicalLeftOffsetForContent();
        if (writingMode().isLogicalLeftInlineStart())
            contentLogicalLeft += textIndentOffset();
        else
            contentLogicalLeft -= textIndentOffset();

        RefPtr button = uploadButton();
        if (!button)
            return;

        LayoutUnit buttonLogicalWidth = nodeLogicalWidth(button);
        LayoutUnit buttonAndIconLogicalWidth = buttonLogicalWidth + afterButtonSpacing
            + (inputElement().icon() ? iconLogicalWidth + iconFilenameSpacing : 0);
        LayoutUnit textLogicalLeft;
        if (writingMode().isLogicalLeftInlineStart())
            textLogicalLeft = contentLogicalLeft + buttonAndIconLogicalWidth;
        else
            textLogicalLeft = contentLogicalLeft + contentBoxLogicalWidth() - buttonAndIconLogicalWidth - font.width(textRun);

        // We want to match the button's baseline
        // FIXME: Make this work with transforms.
        auto textLogicalTop = [&]() -> float {
            if (auto* buttonRenderer = downcast<RenderButton>(button->renderer())) {
                if (auto* buttonTextRenderer = buttonRenderer->textRenderer()) {
                    if (auto textBox = InlineIterator::lineLeftmostTextBoxFor(*buttonTextRenderer)) {
                        auto textVisualRect = textBox->visualRectIgnoringBlockDirection();
                        textVisualRect.setLocation(buttonTextRenderer->localToContainerPoint(textVisualRect.location(), this));
                        textVisualRect.moveBy(roundPointToDevicePixels(paintOffset, document().deviceScaleFactor()));

                        auto metrics = textBox->style().fontCascade().metricsOfPrimaryFont();

                        if (!isHorizontalWritingMode) {
                            if (isBlockFlipped)
                                return textVisualRect.x() - (settings().subpixelInlineLayoutEnabled() ? metrics.ascent() : metrics.intAscent());

                            return textVisualRect.x() + (settings().subpixelInlineLayoutEnabled() ? metrics.descent() : metrics.intDescent());
                        }

                        if (isBlockFlipped)
                            return textVisualRect.y() - (settings().subpixelInlineLayoutEnabled() ? metrics.descent() : metrics.intDescent());

                        return textVisualRect.y() + (settings().subpixelInlineLayoutEnabled() ? metrics.ascent() : metrics.intAscent());
                    }
                }
            }
            // File upload button is display: none (see ::file-selector-button).
            return roundToDevicePixel(marginBoxLogicalHeight(containingBlock()->writingMode()), document().deviceScaleFactor());
        }();

        paintInfo.context().setFillColor(style().visitedDependentColorApplyingColorFilter());
        
        // Draw the filename
        {
            GraphicsContextStateSaver stateSaver(paintInfo.context());

            if (writingMode().isLineOverLeft()) {
                textLogicalLeft += font.width(textRun);
                textLogicalTop += (settings().subpixelInlineLayoutEnabled() ? font.metricsOfPrimaryFont().ascent() : font.metricsOfPrimaryFont().intAscent());
            }

            auto textOrigin = roundPointToDevicePixels({ textLogicalLeft, textLogicalTop }, document().deviceScaleFactor());
            if (!isHorizontalWritingMode) {
                textOrigin = textOrigin.transposedPoint();

                paintInfo.context().translate(textOrigin);
                if (writingMode().isLineOverLeft())
                    paintInfo.context().rotate(-piOverTwoFloat);
                else
                    paintInfo.context().rotate(piOverTwoFloat);
                paintInfo.context().translate(-textOrigin);
            }

            paintInfo.context().drawBidiText(font, textRun, textOrigin);
        }

        if (inputElement().icon()) {
            // Determine where the icon should be placed
            LayoutUnit borderAndPaddingOffsetForIcon = (!isHorizontalWritingMode && isBlockFlipped) ? borderAndPaddingAfter() : borderAndPaddingBefore();
            LayoutUnit iconLogicalTop = logicalPaintOffset.y() + borderAndPaddingOffsetForIcon + (contentBoxLogicalHeight() - iconLogicalHeight) / 2;

            if (writingMode().isLineOverLeft())
                iconLogicalTop += (contentBoxLogicalHeight() - iconLogicalHeight) / 2;

            LayoutUnit iconLogicalLeft;
            if (writingMode().isLogicalLeftInlineStart())
                iconLogicalLeft = contentLogicalLeft + buttonLogicalWidth + afterButtonSpacing;
            else
                iconLogicalLeft = contentLogicalLeft + contentBoxLogicalWidth() - buttonLogicalWidth - afterButtonSpacing - iconLogicalWidth;

            IntRect iconRect(iconLogicalLeft, iconLogicalTop, iconLogicalWidth, iconLogicalHeight);
            if (!isHorizontalWritingMode)
                iconRect = iconRect.transposedRect();

#if PLATFORM(COCOA)
            if (RenderButton* buttonRenderer = downcast<RenderButton>(button->renderer())) {
                // Draw the file icon and decorations.
                auto decorationsType = inputElement().files()->length() == 1 ? RenderTheme::FileUploadDecorations::SingleFile : RenderTheme::FileUploadDecorations::MultipleFiles;
                theme().paintFileUploadIconDecorations(*this, *buttonRenderer, paintInfo, iconRect, inputElement().icon(), decorationsType);
            }
#else
            // Draw the file icon
            inputElement().icon()->paint(paintInfo.context(), iconRect);
#endif
        }
    }
}

void RenderFileUploadControl::computeIntrinsicLogicalWidths(LayoutUnit& minLogicalWidth, LayoutUnit& maxLogicalWidth) const
{
    if (shouldApplySizeOrInlineSizeContainment()) {
        if (auto logicalWidth = explicitIntrinsicInnerLogicalWidth()) {
            minLogicalWidth = logicalWidth.value();
            maxLogicalWidth = logicalWidth.value();
        }
        return;
    }
    // Figure out how big the filename space needs to be for a given number of characters
    // (using "0" as the nominal character).
    const char16_t character = '0';
    const String characterAsString = span(character);
    const FontCascade& font = style().fontCascade();
    // FIXME: Remove the need for this const_cast by making constructTextRun take a const RenderObject*.
    float minDefaultLabelWidth = defaultWidthNumChars * font.width(constructTextRun(characterAsString, style(), ExpansionBehavior::allowRightOnly()));

    const String label = theme().fileListDefaultLabel(inputElement().multiple());
    float defaultLabelWidth = font.width(constructTextRun(label, style(), ExpansionBehavior::allowRightOnly()));
    if (RefPtr button = uploadButton()) {
        if (CheckedPtr buttonRenderer = dynamicDowncast<RenderBox>(button->renderer()))
            defaultLabelWidth += buttonRenderer->maxPreferredLogicalWidth() + afterButtonSpacing;
    }
    maxLogicalWidth = static_cast<int>(ceilf(std::max(minDefaultLabelWidth, defaultLabelWidth)));

    auto& logicalWidth = style().logicalWidth();
    if (logicalWidth.isCalculated())
        minLogicalWidth = std::max(0_lu, Style::evaluate<LayoutUnit>(logicalWidth, 0_lu, style().usedZoomForLength()));
    else if (!logicalWidth.isPercent())
        minLogicalWidth = maxLogicalWidth;
}

void RenderFileUploadControl::computePreferredLogicalWidths()
{
    ASSERT(needsPreferredLogicalWidthsUpdate());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (auto fixedLogicalWidth = style().logicalWidth().tryFixed(); fixedLogicalWidth && fixedLogicalWidth->isPositive())
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(*fixedLogicalWidth);
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    RenderBox::computePreferredLogicalWidths(style().logicalMinWidth(), style().logicalMaxWidth(), writingMode().isHorizontal() ? horizontalBorderAndPaddingExtent() : verticalBorderAndPaddingExtent());

    clearNeedsPreferredWidthsUpdate();
}

PositionWithAffinity RenderFileUploadControl::positionForPoint(const LayoutPoint&, HitTestSource, const RenderFragmentContainer*)
{
    return PositionWithAffinity();
}

HTMLInputElement* RenderFileUploadControl::uploadButton() const
{
    ASSERT(inputElement().shadowRoot());
    return dynamicDowncast<HTMLInputElement>(inputElement().shadowRoot()->firstChild());
}

String RenderFileUploadControl::buttonValue()
{
    if (RefPtr button = uploadButton())
        return button->value();
    
    return String();
}

String RenderFileUploadControl::fileTextValue() const
{
    Ref input = inputElement();
    if (!input->files())
        return { };
    if (input->files()->length() && !input->displayString().isEmpty()) {
        if (input->files()->length() == 1)
            return StringTruncator::centerTruncate(input->displayString(), maxFilenameLogicalWidth(), style().fontCascade());

        return StringTruncator::rightTruncate(input->displayString(), maxFilenameLogicalWidth(), style().fontCascade());
    }
    return theme().fileListNameForWidth(input->files(), style().fontCascade(), maxFilenameLogicalWidth(), input->multiple());
}
    
} // namespace WebCore
