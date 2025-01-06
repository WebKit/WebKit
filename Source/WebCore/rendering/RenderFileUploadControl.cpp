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

#include "ElementRareData.h"
#include "FileList.h"
#include "FontCascade.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Icon.h"
#include "InlineIteratorInlineBox.h"
#include "LocalizedStrings.h"
#include "PaintInfo.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderButton.h"
#include "RenderElementInlines.h"
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

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(RenderFileUploadControl);

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
    : RenderBlockFlow(Type::FileUploadControl, input, WTFMove(style))
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
    return std::max(0, roundToInt(contentLogicalWidth()) - nodeLogicalWidth(uploadButton()) - afterButtonSpacing
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
        if (writingMode().isBidiLTR())
            contentLogicalLeft += textIndentOffset();
        else
            contentLogicalLeft -= textIndentOffset();

        HTMLInputElement* button = uploadButton();
        if (!button)
            return;

        LayoutUnit buttonLogicalWidth = nodeLogicalWidth(button);
        LayoutUnit buttonAndIconLogicalWidth = buttonLogicalWidth + afterButtonSpacing
            + (inputElement().icon() ? iconLogicalWidth + iconFilenameSpacing : 0);
        LayoutUnit textLogicalLeft;
        if (writingMode().isBidiLTR())
            textLogicalLeft = contentLogicalLeft + buttonAndIconLogicalWidth;
        else
            textLogicalLeft = contentLogicalLeft + contentLogicalWidth() - buttonAndIconLogicalWidth - font.width(textRun);

        // We want to match the button's baseline
        // FIXME: Make this work with transforms.
        auto textLogicalTop = [&]() -> float {
            if (auto* buttonRenderer = downcast<RenderButton>(button->renderer())) {
                if (auto* buttonTextRenderer = buttonRenderer->textRenderer()) {
                    if (auto textBox = InlineIterator::firstTextBoxFor(*buttonTextRenderer)) {
                        auto textVisualRect = textBox->visualRectIgnoringBlockDirection();
                        textVisualRect.setLocation(buttonTextRenderer->localToContainerPoint(textVisualRect.location(), this));
                        textVisualRect.moveBy(roundPointToDevicePixels(paintOffset, document().deviceScaleFactor()));

                        auto metrics = textBox->style().fontCascade().metricsOfPrimaryFont();

                        if (!isHorizontalWritingMode) {
                            if (isBlockFlipped)
                                return textVisualRect.x() - metrics.intAscent();

                            return textVisualRect.x() + metrics.intDescent();
                        }

                        if (isBlockFlipped)
                            return textVisualRect.y() - metrics.intDescent();

                        return textVisualRect.y() + metrics.intAscent();
                    }
                }
            }

            return roundToInt(baselinePosition(AlphabeticBaseline, true, isHorizontalWritingMode ? HorizontalLine : VerticalLine, PositionOnContainingLine));
        }();

        paintInfo.context().setFillColor(style().visitedDependentColorWithColorFilter(CSSPropertyColor));
        
        // Draw the filename
        {
            GraphicsContextStateSaver stateSaver(paintInfo.context());

            auto textOrigin = IntPoint(roundToInt(textLogicalLeft), std::round(textLogicalTop));
            if (!isHorizontalWritingMode) {
                textOrigin = textOrigin.transposedPoint();
                paintInfo.context().translate(textOrigin);
                paintInfo.context().rotate(piOverTwoFloat);
                paintInfo.context().translate(-textOrigin);
            }

            paintInfo.context().drawBidiText(font, textRun, textOrigin);
        }

        if (inputElement().icon()) {
            // Determine where the icon should be placed
            LayoutUnit borderAndPaddingOffsetForIcon = (!isHorizontalWritingMode && isBlockFlipped) ? borderAndPaddingAfter() : borderAndPaddingBefore();
            LayoutUnit iconLogicalTop = logicalPaintOffset.y() + borderAndPaddingOffsetForIcon  + (contentLogicalHeight() - iconLogicalHeight) / 2;
            LayoutUnit iconLogicalLeft;
            if (writingMode().isBidiLTR())
                iconLogicalLeft = contentLogicalLeft + buttonLogicalWidth + afterButtonSpacing;
            else
                iconLogicalLeft = contentLogicalLeft + contentLogicalWidth() - buttonLogicalWidth - afterButtonSpacing - iconLogicalWidth;

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
    const UChar character = '0';
    const String characterAsString = span(character);
    const FontCascade& font = style().fontCascade();
    // FIXME: Remove the need for this const_cast by making constructTextRun take a const RenderObject*.
    float minDefaultLabelWidth = defaultWidthNumChars * font.width(constructTextRun(characterAsString, style(), ExpansionBehavior::allowRightOnly()));

    const String label = theme().fileListDefaultLabel(inputElement().multiple());
    float defaultLabelWidth = font.width(constructTextRun(label, style(), ExpansionBehavior::allowRightOnly()));
    if (HTMLInputElement* button = uploadButton())
        if (RenderObject* buttonRenderer = button->renderer())
            defaultLabelWidth += buttonRenderer->maxPreferredLogicalWidth() + afterButtonSpacing;
    maxLogicalWidth = static_cast<int>(ceilf(std::max(minDefaultLabelWidth, defaultLabelWidth)));

    auto& logicalWidth = style().logicalWidth();
    if (logicalWidth.isCalculated())
        minLogicalWidth = std::max(0_lu, valueForLength(logicalWidth, 0_lu));
    else if (!logicalWidth.isPercent())
        minLogicalWidth = maxLogicalWidth;
}

void RenderFileUploadControl::computePreferredLogicalWidths()
{
    ASSERT(preferredLogicalWidthsDirty());

    m_minPreferredLogicalWidth = 0;
    m_maxPreferredLogicalWidth = 0;

    if (style().logicalWidth().isFixed() && style().logicalWidth().value() > 0)
        m_minPreferredLogicalWidth = m_maxPreferredLogicalWidth = adjustContentBoxLogicalWidthForBoxSizing(style().logicalWidth());
    else
        computeIntrinsicLogicalWidths(m_minPreferredLogicalWidth, m_maxPreferredLogicalWidth);

    RenderBox::computePreferredLogicalWidths(style().logicalMinWidth(), style().logicalMaxWidth(), writingMode().isHorizontal() ? horizontalBorderAndPaddingExtent() : verticalBorderAndPaddingExtent());

    setPreferredLogicalWidthsDirty(false);
}

VisiblePosition RenderFileUploadControl::positionForPoint(const LayoutPoint&, HitTestSource, const RenderFragmentContainer*)
{
    return VisiblePosition();
}

HTMLInputElement* RenderFileUploadControl::uploadButton() const
{
    ASSERT(inputElement().shadowRoot());
    return dynamicDowncast<HTMLInputElement>(inputElement().shadowRoot()->firstChild());
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
    if (!input.files())
        return { };
    if (input.files()->length() && !input.displayString().isEmpty()) {
        if (input.files()->length() == 1)
            return StringTruncator::centerTruncate(input.displayString(), maxFilenameLogicalWidth(), style().fontCascade());

        return StringTruncator::rightTruncate(input.displayString(), maxFilenameLogicalWidth(), style().fontCascade());
    }
    return theme().fileListNameForWidth(input.files(), style().fontCascade(), maxFilenameLogicalWidth(), input.multiple());
}
    
} // namespace WebCore
