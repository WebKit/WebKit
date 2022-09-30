/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "InspectorOverlayLabel.h"

#include "FloatRoundedRect.h"
#include "FloatSize.h"
#include "FontCascade.h"
#include "FontCascadeDescription.h"
#include "GraphicsContext.h"
#include "Path.h"

namespace WebCore {

static constexpr float labelPadding = 4;
static constexpr float labelArrowSize = 6;
static constexpr float labelAdditionalLineSpacing = 1;
static constexpr float labelContentDecorationBorderedLeadingAndTrailingPadding = 1;

InspectorOverlayLabel::InspectorOverlayLabel(Vector<Content>&& contents, FloatPoint location, Color backgroundColor, Arrow arrow)
    : m_contents(WTFMove(contents))
    , m_location(location)
    , m_backgroundColor(backgroundColor)
    , m_arrow(arrow)
{
}

InspectorOverlayLabel::InspectorOverlayLabel(const String& text, FloatPoint location, Color backgroundColor, Arrow arrow)
    : InspectorOverlayLabel({ { text, Color::black } }, location, backgroundColor, arrow)
{
}

static FontCascade systemFont()
{
    FontCascadeDescription fontDescription;
    fontDescription.setFamilies({ "system-ui"_s });
    fontDescription.setWeight(FontSelectionValue(500));
    fontDescription.setComputedSize(12);

    FontCascade font(WTFMove(fontDescription), 0, 0);
    font.update(nullptr);
    return font;
}

static Path backgroundPath(float width, float height, InspectorOverlayLabel::Arrow arrow, float arrowSize)
{
    Path path;
    FloatSize offsetForArrowEdgePosition;

    switch (arrow.direction) {
    case InspectorOverlayLabel::Arrow::Direction::Down:
        path.moveTo({ -(width / 2), -height - arrowSize });
        path.addLineTo({ -(width / 2), -arrowSize });

        switch (arrow.alignment) {
        case InspectorOverlayLabel::Arrow::Alignment::Leading:
            path.addLineTo({ -(width / 2), 0 });
            path.addLineTo({ -(width / 2) + arrowSize, -arrowSize });
            offsetForArrowEdgePosition = { (width / 2), 0 };
            break;
        case InspectorOverlayLabel::Arrow::Alignment::Middle:
            path.addLineTo({ -arrowSize, -arrowSize });
            path.addLineTo({ 0, 0 });
            path.addLineTo({ arrowSize, -arrowSize });
            break;
        case InspectorOverlayLabel::Arrow::Alignment::Trailing:
            path.addLineTo({ (width / 2) - arrowSize, -arrowSize });
            path.addLineTo({ (width / 2), 0 });
            offsetForArrowEdgePosition = { -(width / 2), 0 };
            break;
        case InspectorOverlayLabel::Arrow::Alignment::None:
            break;
        }

        path.addLineTo({ (width / 2), -arrowSize });
        path.addLineTo({ (width / 2), -height - arrowSize });
        break;
    case InspectorOverlayLabel::Arrow::Direction::Up:
        path.moveTo({ -(width / 2), height + arrowSize });
        path.addLineTo({ -(width / 2), arrowSize });

        switch (arrow.alignment) {
        case InspectorOverlayLabel::Arrow::Alignment::Leading:
            path.addLineTo({ -(width / 2), 0 });
            path.addLineTo({ -(width / 2) + arrowSize, arrowSize });
            offsetForArrowEdgePosition = { (width / 2), 0 };
            break;
        case InspectorOverlayLabel::Arrow::Alignment::Middle:
            path.addLineTo({ -arrowSize, arrowSize });
            path.addLineTo({ 0, 0 });
            path.addLineTo({ arrowSize, arrowSize });
            break;
        case InspectorOverlayLabel::Arrow::Alignment::Trailing:
            path.addLineTo({ (width / 2) - arrowSize, arrowSize });
            path.addLineTo({ (width / 2), 0 });
            offsetForArrowEdgePosition = { -(width / 2), 0 };
            break;
        case InspectorOverlayLabel::Arrow::Alignment::None:
            break;
        }

        path.addLineTo({ (width / 2), arrowSize });
        path.addLineTo({ (width / 2), height + arrowSize });
        break;
    case InspectorOverlayLabel::Arrow::Direction::Right:
        path.moveTo({ -width - arrowSize, (height / 2) });
        path.addLineTo({ -arrowSize, (height / 2) });

        switch (arrow.alignment) {
        case InspectorOverlayLabel::Arrow::Alignment::Leading:
            path.addLineTo({ -arrowSize, -(height / 2) + arrowSize });
            path.addLineTo({ 0, -(height / 2) });
            offsetForArrowEdgePosition = { 0, (height / 2) };
            break;
        case InspectorOverlayLabel::Arrow::Alignment::Middle:
            path.addLineTo({ -arrowSize, arrowSize });
            path.addLineTo({ 0, 0 });
            path.addLineTo({ -arrowSize, -arrowSize });
            break;
        case InspectorOverlayLabel::Arrow::Alignment::Trailing:
            path.addLineTo({ 0, (height / 2) });
            path.addLineTo({ -arrowSize, (height / 2) - arrowSize });
            offsetForArrowEdgePosition = { 0, -(height / 2) };
            break;
        case InspectorOverlayLabel::Arrow::Alignment::None:
            break;
        }

        path.addLineTo({ -arrowSize, -(height / 2) });
        path.addLineTo({ -width - arrowSize, -(height / 2) });
        break;
    case InspectorOverlayLabel::Arrow::Direction::Left:
        path.moveTo({ width + arrowSize, (height / 2) });
        path.addLineTo({ arrowSize, (height / 2) });

        switch (arrow.alignment) {
        case InspectorOverlayLabel::Arrow::Alignment::Leading:
            path.addLineTo({ arrowSize, -(height / 2) + arrowSize });
            path.addLineTo({ 0, -(height / 2) });
            offsetForArrowEdgePosition = { 0, (height / 2) };
            break;
        case InspectorOverlayLabel::Arrow::Alignment::Middle:
            path.addLineTo({ arrowSize, arrowSize });
            path.addLineTo({ 0, 0 });
            path.addLineTo({ arrowSize, -arrowSize });
            break;
        case InspectorOverlayLabel::Arrow::Alignment::Trailing:
            path.addLineTo({ 0, (height / 2) });
            path.addLineTo({ arrowSize, (height / 2) - arrowSize });
            offsetForArrowEdgePosition = { 0, -(height / 2) };
            break;
        case InspectorOverlayLabel::Arrow::Alignment::None:
            break;
        }

        path.addLineTo({ arrowSize, -(height / 2) });
        path.addLineTo({ width + arrowSize, -(height / 2) });
        break;
    case InspectorOverlayLabel::Arrow::Direction::None:
        path.moveTo({ -(width / 2), -(height / 2) });
        path.addLineTo({ -(width / 2), height / 2 });
        path.addLineTo({ width / 2, height / 2 });
        path.addLineTo({ width / 2, -(height / 2) });
        break;
    }

    path.closeSubpath();
    path.translate(offsetForArrowEdgePosition);

    return path;
}

struct ComputedContentRun {
    TextRun textRun;
    Color textColor;
    InspectorOverlayLabel::Content::Decoration decoration;
    bool startsNewLine;
    float computedWidth;
};

Path InspectorOverlayLabel::draw(GraphicsContext& context, float maximumLineWidth)
{
    constexpr UChar ellipsis = 0x2026;

    auto font = systemFont();
    float lineHeight = font.metricsOfPrimaryFont().floatHeight();
    float lineDescent = font.metricsOfPrimaryFont().floatDescent();

    Vector<ComputedContentRun> computedContentRuns;

    float longestLineWidth = 0;
    int currentLine = 0;

    float currentLineWidth = 0;
    for (auto content : m_contents) {
        auto lines = content.text.splitAllowingEmptyEntries('\n');

        ASSERT(content.decoration.type == Content::Decoration::Type::None || lines.size() <= 1);

        for (size_t i = 0; i < lines.size(); ++i) {
            auto startsNewLine = !!i;
            if (startsNewLine) {
                currentLineWidth = 0;
                ++currentLine;
            }

            auto text = lines[i];
            auto textRun = TextRun(text);
            float textWidth = font.width(textRun);

            // FIXME: This looks very inefficient.
            if (maximumLineWidth && currentLineWidth + textWidth + (labelPadding * 2) > maximumLineWidth) {
                text = makeString(text, ellipsis);
                while (currentLineWidth + textWidth + (labelPadding * 2) > maximumLineWidth && text.length() > 1) {
                    // Remove the second from last character (the character before the ellipsis) and remeasure.
                    text = makeStringByRemoving(text, text.length() - 2, 1);
                    textRun = TextRun(text);
                    textWidth = font.width(textRun);
                }
            }

            computedContentRuns.append({ textRun, content.textColor, content.decoration, startsNewLine, textWidth });

            currentLineWidth += textWidth;
            if (currentLineWidth > longestLineWidth)
                longestLineWidth = currentLineWidth;
        }
    }

    float totalTextHeight = (lineHeight * (currentLine + 1)) + (currentLine * labelAdditionalLineSpacing);

    FloatPoint textPosition;
    switch (m_arrow.direction) {
    case Arrow::Direction::Down:
        switch (m_arrow.alignment) {
        case Arrow::Alignment::Leading:
            textPosition = FloatPoint(labelPadding, lineHeight - totalTextHeight - lineDescent - labelArrowSize - labelPadding);
            break;
        case Arrow::Alignment::Middle:
            textPosition = FloatPoint(-(longestLineWidth / 2), lineHeight - totalTextHeight - lineDescent - labelArrowSize - labelPadding);
            break;
        case Arrow::Alignment::Trailing:
            textPosition = FloatPoint(-longestLineWidth - labelPadding, lineHeight - totalTextHeight - lineDescent - labelArrowSize - labelPadding);
            break;
        case Arrow::Alignment::None:
            break;
        }
        break;
    case Arrow::Direction::Up:
        switch (m_arrow.alignment) {
        case Arrow::Alignment::Leading:
            textPosition = FloatPoint(labelPadding, lineHeight - lineDescent + labelArrowSize + labelPadding);
            break;
        case Arrow::Alignment::Middle:
            textPosition = FloatPoint(-(longestLineWidth / 2), lineHeight - lineDescent + labelArrowSize + labelPadding);
            break;
        case Arrow::Alignment::Trailing:
            textPosition = FloatPoint(-longestLineWidth - labelPadding, lineHeight - lineDescent + labelArrowSize + labelPadding);
            break;
        case Arrow::Alignment::None:
            break;
        }
        break;
    case Arrow::Direction::Right:
        switch (m_arrow.alignment) {
        case Arrow::Alignment::Leading:
            textPosition = FloatPoint(-longestLineWidth - labelArrowSize - labelPadding, labelPadding + lineHeight - lineDescent);
            break;
        case Arrow::Alignment::Middle:
            textPosition = FloatPoint(-longestLineWidth - labelArrowSize - labelPadding, lineHeight - (totalTextHeight / 2) - lineDescent);
            break;
        case Arrow::Alignment::Trailing:
            textPosition = FloatPoint(-longestLineWidth - labelArrowSize - labelPadding, lineHeight - totalTextHeight - labelPadding - lineDescent);
            break;
        case Arrow::Alignment::None:
            break;
        }
        break;
    case Arrow::Direction::Left:
        switch (m_arrow.alignment) {
        case Arrow::Alignment::Leading:
            textPosition = FloatPoint(labelArrowSize + labelPadding, labelPadding + lineHeight - lineDescent);
            break;
        case Arrow::Alignment::Middle:
            textPosition = FloatPoint(labelArrowSize + labelPadding, lineHeight - (totalTextHeight / 2) - lineDescent);
            break;
        case Arrow::Alignment::Trailing:
            textPosition = FloatPoint(labelArrowSize + labelPadding, lineHeight - totalTextHeight - labelPadding - lineDescent);
            break;
        case Arrow::Alignment::None:
            break;
        }
        break;
    case Arrow::Direction::None:
        textPosition = FloatPoint(-(longestLineWidth / 2), -(totalTextHeight / 2) + lineHeight - lineDescent);
        break;
    }

    Path labelPath = backgroundPath(longestLineWidth + (labelPadding * 2), totalTextHeight + (labelPadding * 2), m_arrow, labelArrowSize);

    GraphicsContextStateSaver saver(context);
    context.translate(m_location);

    context.setFillColor(m_backgroundColor);
    context.fillPath(labelPath);
    context.strokePath(labelPath);

    float xOffset = 0;
    float yOffset = 0;
    for (auto& computedContentRun : computedContentRuns) {
        if (computedContentRun.startsNewLine) {
            xOffset = 0;
            yOffset += lineHeight + labelAdditionalLineSpacing;
        }

        switch (computedContentRun.decoration.type) {
        case Content::Decoration::Type::Bordered: {
            auto backgroundRect = FloatRoundedRect({
                textPosition.x() + xOffset - labelContentDecorationBorderedLeadingAndTrailingPadding,
                textPosition.y() + yOffset - lineHeight + lineDescent,
                computedContentRun.computedWidth + (labelContentDecorationBorderedLeadingAndTrailingPadding * 2),
                lineHeight,
            }, FloatRoundedRect::Radii(2));

            Path backgroundPath;
            backgroundPath.addRoundedRect(backgroundRect);

            context.setFillColor(computedContentRun.decoration.color);
            context.setStrokeColor(computedContentRun.decoration.color.darkened());

            context.fillPath(backgroundPath);
            context.strokePath(backgroundPath);
            break;
        }

        case Content::Decoration::Type::None:
            break;
        }

        context.setFillColor(computedContentRun.textColor);
        context.drawText(font, computedContentRun.textRun, textPosition + FloatPoint(xOffset, yOffset));

        xOffset += computedContentRun.computedWidth;
    }

    return labelPath;
}

FloatSize InspectorOverlayLabel::expectedSize(const Vector<Content>& contents, Arrow::Direction direction)
{
    auto font = systemFont();
    float lineHeight = font.metricsOfPrimaryFont().floatHeight();

    float longestLineWidth = 0;
    int currentLine = 0;

    float currentLineWidth = 0;
    for (auto content : contents) {
        auto lines = content.text.splitAllowingEmptyEntries('\n');
        for (size_t i = 0; i < lines.size(); ++i) {
            if (i) {
                currentLineWidth = 0;
                ++currentLine;
            }

            auto text = lines[i];
            if (text.isEmpty())
                continue;

            currentLineWidth += font.width(TextRun(text));
            if (currentLineWidth > longestLineWidth)
                longestLineWidth = currentLineWidth;
        }
    }

    float totalTextHeight = (lineHeight * (currentLine + 1)) + (currentLine * labelAdditionalLineSpacing);

    switch (direction) {
    case Arrow::Direction::Down:
    case Arrow::Direction::Up:
        return { longestLineWidth + (labelPadding * 2), totalTextHeight + (labelPadding * 2) + labelArrowSize };
    case Arrow::Direction::Right:
    case Arrow::Direction::Left:
        return { longestLineWidth + (labelPadding * 2) + labelArrowSize, totalTextHeight + (labelPadding * 2) };
    case Arrow::Direction::None:
        return { longestLineWidth + (labelPadding * 2), totalTextHeight + (labelPadding * 2) };
    }

    RELEASE_ASSERT_NOT_REACHED();
}

FloatSize InspectorOverlayLabel::expectedSize(const String& text, Arrow::Direction direction)
{
    return InspectorOverlayLabel::expectedSize({ { text, Color::black } }, direction);
}

} // namespace WebCore
