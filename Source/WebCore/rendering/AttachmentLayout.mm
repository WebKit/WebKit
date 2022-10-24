/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "AttachmentLayout.h"

#if ENABLE(ATTACHMENT_ELEMENT) && PLATFORM(COCOA)

#include "ColorCocoa.h"
#include "FontCacheCoreText.h"
#include "FrameSelection.h"
#include "GeometryUtilities.h"
#include "RenderTheme.h"
#include <pal/spi/cf/CoreTextSPI.h>

namespace WebCore {

#if PLATFORM(MAC)

const CGFloat attachmentIconSize = 48;
const CGFloat attachmentIconBackgroundPadding = 6;
const CGFloat attachmentIconBackgroundSize = attachmentIconSize + attachmentIconBackgroundPadding;
const CGFloat attachmentIconSelectionBorderThickness = 1;
const CGFloat attachmentIconBackgroundRadius = 3;
const CGFloat attachmentIconToTitleMargin = 2;

constexpr auto attachmentIconBackgroundColor = Color::black.colorWithAlphaByte(30);
constexpr auto attachmentIconBorderColor = Color::white.colorWithAlphaByte(125);

const CGFloat attachmentTitleFontSize = 12;
const CGFloat attachmentTitleBackgroundRadius = 3;
const CGFloat attachmentTitleBackgroundPadding = 3;
const CGFloat attachmentTitleMaximumWidth = 100 - (attachmentTitleBackgroundPadding * 2);
const CFIndex attachmentTitleMaximumLineCount = 2;

constexpr auto attachmentTitleInactiveBackgroundColor = SRGBA<uint8_t> { 204, 204, 204 };
constexpr auto attachmentTitleInactiveTextColor = SRGBA<uint8_t> { 100, 100, 100 };

const CGFloat attachmentSubtitleFontSize = 10;
const int attachmentSubtitleWidthIncrement = 10;
constexpr auto attachmentSubtitleTextColor = SRGBA<uint8_t> { 82, 145, 214 };

const CGFloat attachmentProgressBarWidth = 30;
const CGFloat attachmentProgressBarHeight = 5;
const CGFloat attachmentProgressBarOffset = -9;
const CGFloat attachmentProgressBarBorderWidth = 1;
constexpr auto attachmentProgressBarBackgroundColor = Color::black.colorWithAlphaByte(89);
constexpr auto attachmentProgressBarFillColor = Color::white;
constexpr auto attachmentProgressBarBorderColor = Color::black.colorWithAlphaByte(128);

const CGFloat attachmentPlaceholderBorderRadius = 5;
constexpr auto attachmentPlaceholderBorderColor = Color::black.colorWithAlphaByte(56);
const CGFloat attachmentPlaceholderBorderWidth = 2;
const CGFloat attachmentPlaceholderBorderDashLength = 6;
const CGFloat attachmentMargin = 3;

static Color titleTextColorForAttachment(const RenderAttachment& attachment, AttachmentLayoutStyle style)
{
    Color result = Color::black;
    
    if (style == AttachmentLayoutStyle::Selected) {
        if (attachment.frame().selection().isFocusedAndActive())
            result = colorFromCocoaColor([NSColor alternateSelectedControlTextColor]);
        else
            result = attachmentTitleInactiveTextColor;
    }

    return attachment.style().colorByApplyingColorFilter(result);
}

void AttachmentLayout::layOutTitle(const RenderAttachment& attachment)
{
    CFStringRef language = nullptr; // By not specifying a language we use the system language.
    auto font = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, attachmentTitleFontSize, language));
    baseline = CGRound(attachmentIconBackgroundSize + attachmentIconToTitleMargin + CTFontGetAscent(font.get()));
    wrappingWidth = attachmentTitleMaximumWidth;
    widthPadding = attachmentIconBackgroundSize;
    String title = attachment.attachmentElement().attachmentTitleForDisplay();
    if (title.isEmpty())
        return;
    NSDictionary *textAttributes = @{
        (__bridge id)kCTFontAttributeName: (__bridge id)font.get(),
        (__bridge id)kCTForegroundColorAttributeName: (__bridge id)cachedCGColor(titleTextColorForAttachment(attachment, style)).get()
    };
    buildWrappedLines(title, font.get(), textAttributes, attachmentTitleMaximumLineCount);
    CGFloat yOffset = attachmentIconBackgroundSize + attachmentIconToTitleMargin;
    unsigned i = 0;
    if (!lines.isEmpty()) {
        for (auto& line : lines) {
            if (i)
                yOffset += origins[i - 1].y - origins[i].y;
            line.rect.setY(yOffset);

            line.backgroundRect = LayoutRect(line.rect);
            line.rect.setY(yOffset - origins.last().y);

            line.backgroundRect.inflateX(attachmentTitleBackgroundPadding);
            line.backgroundRect = encloseRectToDevicePixels(line.backgroundRect, attachment.document().deviceScaleFactor());
        
            // If the text rects are close in size, the curved enclosing background won't
            // look right, so make them the same exact size.
            if (i) {
                float previousBackgroundRectWidth = lines[i-1].backgroundRect.width();
                if (fabs(line.backgroundRect.width() - previousBackgroundRectWidth) < attachmentTitleBackgroundRadius * 4) {
                    float newBackgroundRectWidth = std::max(previousBackgroundRectWidth, line.backgroundRect.width());
                    line.backgroundRect.inflateX((newBackgroundRectWidth - line.backgroundRect.width()) / 2);
                    lines[i-1].backgroundRect.inflateX((newBackgroundRectWidth - previousBackgroundRectWidth) / 2);
                }
            }
            i++;
        }
    }
}

void AttachmentLayout::layOutSubtitle(const RenderAttachment& attachment)
{
    auto& subtitleText = attachment.attachmentElement().attributeWithoutSynchronization(HTMLNames::subtitleAttr);
    if (subtitleText.isEmpty())
        return;
    auto subtitleColor = attachment.style().colorByApplyingColorFilter(attachmentSubtitleTextColor);
    CFStringRef language = nullptr; // By not specifying a language we use the system language.
    auto font = adoptCF(CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, attachmentSubtitleFontSize, language));
    NSDictionary *textAttributes = @{
        (__bridge id)kCTFontAttributeName: (__bridge id)font.get(),
        (__bridge id)kCTForegroundColorAttributeName: (__bridge id)cachedCGColor(subtitleColor).get()
    };
    CGFloat yOffset = 0;
    if (!lines.isEmpty())
        yOffset = lines.last().backgroundRect.maxY();
    else
        yOffset = attachmentIconBackgroundSize + attachmentIconToTitleMargin;
        
    buildSingleLine(subtitleText, font.get(), textAttributes);
    lines.last().rect.setY(yOffset);
    
    subtitleTextRect = LayoutRect(lines.last().rect);
    lines.last().rect.setLocation(subtitleTextRect.minXMaxYCorner());
    lines.last().rect.setSize(FloatSize(0, 0));
}

AttachmentLayout::AttachmentLayout(const RenderAttachment& attachment, AttachmentLayoutStyle layoutStyle)
    : style(layoutStyle)
{
    excludeTypographicLeading = false;
    layOutTitle(attachment);
    layOutSubtitle(attachment);

    iconBackgroundRect = FloatRect(0, 0, attachmentIconBackgroundSize, attachmentIconBackgroundSize);

    iconRect = iconBackgroundRect;
    iconRect.setSize(FloatSize(attachmentIconSize, attachmentIconSize));
    iconRect.move(attachmentIconBackgroundPadding / 2, attachmentIconBackgroundPadding / 2);

    attachmentRect = iconBackgroundRect;
    for (const auto& line : lines)
        attachmentRect.unite(line.backgroundRect);
    if (!subtitleTextRect.isEmpty()) {
        FloatRect roundedSubtitleTextRect = subtitleTextRect;
        roundedSubtitleTextRect.inflateX(attachmentSubtitleWidthIncrement - clampToInteger(ceilf(subtitleTextRect.width())) % attachmentSubtitleWidthIncrement);
        attachmentRect.unite(roundedSubtitleTextRect);
    }
    attachmentRect.inflate(attachmentMargin);
    attachmentRect = encloseRectToDevicePixels(attachmentRect, attachment.document().deviceScaleFactor());
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

const CGSize attachmentSize = { 160, 119 };

const CGFloat attachmentBorderRadius = 16;
constexpr auto attachmentBorderColor = SRGBA<uint8_t> { 204, 204, 204 };
static CGFloat attachmentBorderThickness = 1;

constexpr auto attachmentProgressColor = SRGBA<uint8_t> { 222, 222, 222 };
const CGFloat attachmentProgressBorderThickness = 3;

const CGFloat attachmentProgressSize = 36;
const CGFloat attachmentIconSize = 48;

const CGFloat attachmentItemMargin = 8;

const CGFloat attachmentWrappingTextMaximumWidth = 140;
const CFIndex attachmentWrappingTextMaximumLineCount = 2;

static BOOL getAttachmentProgress(const RenderAttachment& attachment, float& progress)
{
    auto& progressString = attachment.attachmentElement().attributeWithoutSynchronization(HTMLNames::progressAttr);
    if (progressString.isEmpty())
        return NO;
    bool validProgress;
    progress = std::max<float>(std::min<float>(progressString.toFloat(&validProgress), 1), 0);
    return validProgress;
}

static RetainPtr<CTFontRef> attachmentActionFont()
{
    auto style = kCTUIFontTextStyleFootnote;
    auto size = contentSizeCategory();
    auto attributes = static_cast<CFDictionaryRef>(@{ (id)kCTFontTraitsAttribute: @{ (id)kCTFontSymbolicTrait: @(kCTFontTraitTightLeading | kCTFontTraitEmphasized) } });
#if HAVE(CTFONTDESCRIPTOR_CREATE_WITH_TEXT_STYLE_AND_ATTRIBUTES)
    auto emphasizedFontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyleAndAttributes(style, size, attributes));
#else
    auto fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(style, size, 0));
    auto emphasizedFontDescriptor = adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor.get(), attributes));
#endif

    return adoptCF(CTFontCreateWithFontDescriptor(emphasizedFontDescriptor.get(), 0, nullptr));
}

static RetainPtr<UIColor> attachmentActionColor(const RenderAttachment& attachment)
{
    return cocoaColor(attachment.style().visitedDependentColor(CSSPropertyColor));
}

static RetainPtr<CTFontRef> attachmentTitleFont()
{
    auto fontDescriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(kCTUIFontTextStyleShortCaption1, contentSizeCategory(), 0));
    return adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor.get(), 0, nullptr));
}

static UIColor *attachmentTitleColor(const RenderAttachment& renderer)
{
    return cocoaColor(RenderTheme::singleton().systemColor(CSSValueAppleSystemGray, renderer.styleColorOptions())).autorelease();
}

static RetainPtr<CTFontRef> attachmentSubtitleFont() { return attachmentTitleFont(); }

static UIColor *attachmentSubtitleColor(const RenderAttachment& renderer) { return attachmentTitleColor(renderer); }

static CGFloat shortCaptionPointSizeWithContentSizeCategory(CFStringRef contentSizeCategory)
{
    auto descriptor = adoptCF(CTFontDescriptorCreateWithTextStyle(kCTUIFontTextStyleShortCaption1, contentSizeCategory, 0));
    auto pointSize = adoptCF(CTFontDescriptorCopyAttribute(descriptor.get(), kCTFontSizeAttribute));
    return [dynamic_objc_cast<NSNumber>((__bridge id)pointSize.get()) floatValue];
}

static CGFloat attachmentDynamicTypeScaleFactor()
{
    CGFloat fixedPointSize = shortCaptionPointSizeWithContentSizeCategory(kCTFontContentSizeCategoryL);
    CGFloat dynamicPointSize = shortCaptionPointSizeWithContentSizeCategory(contentSizeCategory());
    if (!dynamicPointSize || !fixedPointSize)
        return 1;
    return std::max<CGFloat>(1, dynamicPointSize / fixedPointSize);
}

AttachmentLayout::AttachmentLayout(const RenderAttachment& attachment, AttachmentLayoutStyle)
{
    excludeTypographicLeading = true;
    attachmentRect = FloatRect(0, 0, attachment.width().toFloat(), attachment.height().toFloat());
    wrappingWidth = attachmentWrappingTextMaximumWidth * attachmentDynamicTypeScaleFactor();
    widthPadding = attachmentRect.width();

    hasProgress = getAttachmentProgress(attachment, progress);
    String title = attachment.attachmentElement().attachmentTitleForDisplay();
    String action = attachment.attachmentElement().attributeWithoutSynchronization(HTMLNames::actionAttr);
    String subtitle = attachment.attachmentElement().attributeWithoutSynchronization(HTMLNames::subtitleAttr);

    CGFloat yOffset = 0;

    if (hasProgress) {
        progressRect = FloatRect((attachmentRect.width() / 2) - (attachmentProgressSize / 2), 0, attachmentProgressSize, attachmentProgressSize);
        yOffset += attachmentProgressSize + attachmentItemMargin;
    }

    if (action.isEmpty() && !hasProgress) {
        FloatSize iconSize = attachment.attachmentElement().iconSize();
        icon = attachment.attachmentElement().icon();
        if (!icon)
            attachment.attachmentElement().requestIconWithSize(FloatSize());
        thumbnailIcon = attachment.attachmentElement().thumbnail();
        if (thumbnailIcon)
            iconSize = largestRectWithAspectRatioInsideRect(thumbnailIcon->size().aspectRatio(), FloatRect(0, 0, attachmentIconSize, attachmentIconSize)).size();
        
        if (thumbnailIcon || icon) {
            iconRect = FloatRect(FloatPoint((attachmentRect.width() / 2) - (iconSize.width() / 2), 0), iconSize);
            yOffset += iconRect.height() + attachmentItemMargin;
        }
    } else {
        NSDictionary *textAttributesTitle = @{
            (id)kCTFontAttributeName: (id)attachmentActionFont().get(),
            (id)kCTForegroundColorAttributeName: attachmentActionColor(attachment).get()
        };

        buildWrappedLines(action, attachmentActionFont().get(), textAttributesTitle, attachmentWrappingTextMaximumLineCount);
    }

    bool forceSingleLineTitle = !action.isEmpty() || !subtitle.isEmpty() || hasProgress;
    NSDictionary *textAttributesTitle = @{
        (id)kCTFontAttributeName: (id)attachmentTitleFont().get(),
        (id)kCTForegroundColorAttributeName: attachmentTitleColor(attachment)
    };

    buildWrappedLines(title, attachmentTitleFont().get(), textAttributesTitle, forceSingleLineTitle ? 1 : attachmentWrappingTextMaximumLineCount);
    NSDictionary *textAttributesSubTitle = @{
        (id)kCTFontAttributeName: (id)attachmentSubtitleFont().get(),
        (id)kCTForegroundColorAttributeName: attachmentSubtitleColor(attachment)
    };
    buildSingleLine(subtitle, attachmentSubtitleFont().get(), textAttributesSubTitle);

    if (!lines.isEmpty()) {
        for (auto& line : lines) {
            line.rect.setY(yOffset);
            yOffset += line.rect.height() + attachmentItemMargin;
        }
    }

    yOffset -= attachmentItemMargin;

    contentYOrigin = (attachmentRect.height() / 2) - (yOffset / 2);
}

#endif // PLATFORM(IOS_FAMILY)

void AttachmentLayout::addLine(CTFontRef font, CTLineRef line, bool isSubtitle = false)
{
    CGRect lineBounds = CTLineGetBoundsWithOptions(line, excludeTypographicLeading ? kCTLineBoundsExcludeTypographicLeading : 0);
    CGFloat trailingWhitespaceWidth = CTLineGetTrailingWhitespaceWidth(line);
    CGFloat lineWidthIgnoringTrailingWhitespace = lineBounds.size.width - trailingWhitespaceWidth;
    CGFloat lineHeight = lineBounds.size.height + (excludeTypographicLeading ? lineBounds.origin.y : 0);
    lineHeight = isSubtitle ? lineHeight : CGCeiling(lineHeight);
    
    CGFloat xOffset = (widthPadding / 2) - (lineWidthIgnoringTrailingWhitespace / 2);
    LabelLine labelLine;
    labelLine.font = font;
    labelLine.line = line;
    labelLine.rect = FloatRect(xOffset, 0, lineWidthIgnoringTrailingWhitespace, lineHeight);
    
    lines.append(labelLine);
}

void AttachmentLayout::buildWrappedLines(String& text, CTFontRef font, NSDictionary *textAttributes, unsigned maximumLineCount)
{
    if (text.isEmpty())
        return;
    
    auto attributedText = adoptNS([[NSAttributedString alloc] initWithString:text attributes:textAttributes]);
    auto framesetter = adoptCF(CTFramesetterCreateWithAttributedString((CFAttributedStringRef)attributedText.get()));
    
    CFRange fitRange;
    auto textSize = CTFramesetterSuggestFrameSizeWithConstraints(framesetter.get(), CFRangeMake(0, 0), nullptr, CGSizeMake(wrappingWidth, CGFLOAT_MAX), &fitRange);
    
    auto textPath = adoptCF(CGPathCreateWithRect(CGRectMake(0, 0, textSize.width, textSize.height), nullptr));
    auto textFrame = adoptCF(CTFramesetterCreateFrame(framesetter.get(), fitRange, textPath.get(), nullptr));
    
    auto ctLines = CTFrameGetLines(textFrame.get());
    auto lineCount = CFArrayGetCount(ctLines);
    if (!lineCount)
        return;
    
    origins.resize(lineCount);
    CTFrameGetLineOrigins(textFrame.get(), CFRangeMake(0, 0), origins.data());
    // Lay out and record the first (maximumLineCount - 1) lines.
    CFIndex lineIndex = 0;
    auto nonTruncatedLineCount = std::min<CFIndex>(maximumLineCount - 1, lineCount);
    for (; lineIndex < nonTruncatedLineCount; ++lineIndex)
        addLine(font, (CTLineRef)CFArrayGetValueAtIndex(ctLines, lineIndex));
    
    if (lineIndex == lineCount)
        return;
    
    // We had text that didn't fit in the first (maximumLineCount - 1) lines.
    // Combine it into one last line, and center-truncate it.
    auto firstRemainingLine = (CTLineRef)CFArrayGetValueAtIndex(ctLines, lineIndex);
    auto remainingRangeStart = CTLineGetStringRange(firstRemainingLine).location;
    auto remainingRange = CFRangeMake(remainingRangeStart, [attributedText length] - remainingRangeStart);
    auto remainingPath = adoptCF(CGPathCreateWithRect(CGRectMake(0, 0, CGFLOAT_MAX, CGFLOAT_MAX), nullptr));
    auto remainingFrame = adoptCF(CTFramesetterCreateFrame(framesetter.get(), remainingRange, remainingPath.get(), nullptr));
    auto ellipsisString = adoptNS([[NSAttributedString alloc] initWithString:@"\u2026" attributes:textAttributes]);
    auto ellipsisLine = adoptCF(CTLineCreateWithAttributedString((CFAttributedStringRef)ellipsisString.get()));
    auto remainingLine = (CTLineRef)CFArrayGetValueAtIndex(CTFrameGetLines(remainingFrame.get()), 0);
    auto truncatedLine = adoptCF(CTLineCreateTruncatedLine(remainingLine, wrappingWidth, kCTLineTruncationMiddle, ellipsisLine.get()));
    
    if (!truncatedLine)
        truncatedLine = remainingLine;
    
    addLine(font, truncatedLine.get());
}

void AttachmentLayout::buildSingleLine(const String& text, CTFontRef font, NSDictionary *textAttributes)
{
    if (text.isEmpty())
        return;
    RetainPtr<NSAttributedString> attributedText = adoptNS([[NSAttributedString alloc] initWithString:text attributes:textAttributes]);
    
    addLine(font, adoptCF(CTLineCreateWithAttributedString((CFAttributedStringRef)attributedText.get())).get(), true);
}

} // namespace WebCore

#endif // ENABLE(ATTACHMENT_ELEMENT) && PLATFORM(COCOA)
