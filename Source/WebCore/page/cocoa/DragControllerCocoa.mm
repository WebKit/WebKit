/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "DragController.h"

#if ENABLE(DRAG_SUPPORT)

#import "DataTransfer.h"
#import "DeprecatedGlobalSettings.h"
#import "Document.h"
#import "DocumentFragment.h"
#import "DragClient.h"
#import "DragData.h"
#import "Editor.h"
#import "EditorClient.h"
#import "Element.h"
#import "File.h"
#import "FrameDestructionObserverInlines.h"
#import "HTMLAttachmentElement.h"
#import "LocalFrame.h"
#import "LocalFrameView.h"
#import "NodeDocument.h"
#import "Page.h"
#import "Pasteboard.h"
#import "PasteboardStrategy.h"
#import "PlatformStrategies.h"
#import "Range.h"
#import "TextIndicator.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#if PLATFORM(MAC)
#import "ColorMac.h"
#import "LocalDefaultSystemAppearance.h"
#import "WebKitNSImageExtras.h"
#import <pal/spi/cf/CoreTextSPI.h>
#import <pal/spi/cocoa/URLFormattingSPI.h>
#import <wtf/SoftLinking.h>

#if !HAVE(URL_FORMATTING)
SOFT_LINK_PRIVATE_FRAMEWORK_OPTIONAL(LinkPresentation)
#endif
#endif

#if PLATFORM(IOS_FAMILY)
#import "GraphicsContextCG.h"
#import <MobileCoreServices/MobileCoreServices.h>
#import <UIKit/UIGraphicsImageRenderer.h>
#import <pal/ios/UIKitSoftLink.h>
#endif

namespace WebCore {

const int DragController::MaxOriginalImageArea = 1500 * 1500;
const int DragController::DragIconRightInset = 7;
const int DragController::DragIconBottomInset = 3;

const float DragController::DragImageAlpha = 0.75f;

bool LEGACY_NODELETE DragController::isCopyKeyDown(const DragData& dragData)
{
    return dragData.flags().contains(DragApplicationFlags::IsCopyKeyDown);
}

std::optional<DragOperation> DragController::dragOperation(const DragData& dragData)
{
    if (dragData.flags().contains(DragApplicationFlags::IsModal))
        return std::nullopt;

    bool mayContainURL;
    if (canLoadDataFromDraggingPasteboard())
        mayContainURL = dragData.containsURL();
    else
        mayContainURL = dragData.containsURLTypeIdentifier();

    if (!mayContainURL && !dragData.containsPromise())
        return std::nullopt;

    if (!m_documentUnderMouse || (!(dragData.flags().containsAll({ DragApplicationFlags::HasAttachedSheet, DragApplicationFlags::IsSource }))))
        return DragOperation::Copy;

    return std::nullopt;
}

const IntSize& LEGACY_NODELETE DragController::maxDragImageSize()
{
    static const IntSize maxDragImageSize(400, 400);

    return maxDragImageSize;
}

void DragController::cleanupAfterSystemDrag()
{
#if PLATFORM(MAC)
    // Drag has ended, dragEnded *should* have been called, however it is possible
    // for the UIDelegate to take over the drag, and fail to send the appropriate
    // drag termination event. As dragEnded just resets drag variables, we just
    // call it anyway to be on the safe side.
    // We don't want to do this for WebKit2, since the client call to start the drag
    // is asynchronous.

    if (protect(protect(m_page->mainFrame())->virtualView())->platformWidget())
        dragEnded();
#endif
}

#if PLATFORM(IOS_FAMILY)

DragOperation DragController::platformGenericDragOperation()
{
    // On iOS, UIKit skips the -performDrop invocation altogether if MOVE is forbidden.
    // Thus, if MOVE is not allowed in the drag source operation mask, fall back to only other allowable action, COPY.
    return DragOperation::Copy;
}

void DragController::updateSupportedTypeIdentifiersForDragHandlingMethod(DragHandlingMethod dragHandlingMethod, const DragData& dragData) const
{
    Vector<String> supportedTypes;
    switch (dragHandlingMethod) {
    case DragHandlingMethod::PageLoad:
        supportedTypes.append(UTTypeURL.identifier);
        break;
    case DragHandlingMethod::EditPlainText:
        supportedTypes.append(UTTypeURL.identifier);
        supportedTypes.append(UTTypePlainText.identifier);
        break;
    case DragHandlingMethod::EditRichText:
        if (DeprecatedGlobalSettings::attachmentElementEnabled()) {
            supportedTypes.append(WebArchivePboardType);
            supportedTypes.append(UTTypeContent.identifier);
            supportedTypes.append(UTTypeItem.identifier);
        } else {
            for (NSString *type in Pasteboard::supportedWebContentPasteboardTypes())
                supportedTypes.append(type);
        }
        break;
    case DragHandlingMethod::SetColor:
        supportedTypes.append(UIColorPboardType);
        break;
    default:
        for (NSString *type in Pasteboard::supportedFileUploadPasteboardTypes())
            supportedTypes.append(type);
        break;
    }

    auto context = dragData.createPasteboardContext();
    platformStrategies()->pasteboardStrategy()->updateSupportedTypeIdentifiers(supportedTypes, dragData.pasteboardName(), context.get());
}

#endif // PLATFORM(IOS_FAMILY)

void DragController::declareAndWriteDragImage(DataTransfer& dataTransfer, Element& element, const URL& url, const String& label)
{
    client().declareAndWriteDragImage(dataTransfer.pasteboard().name(), element, url, label, protect(protect(element.document())->frame()));
}

#if PLATFORM(MAC)

constexpr CGFloat linkImagePadding = 10;
constexpr CGFloat linkImageDomainBaselineToTitleBaseline = 18;
constexpr CGFloat linkImageCornerRadius = 5;
constexpr CGFloat linkImageMaximumWidth = 400;
constexpr CGFloat linkImageFontSize = 11;
constexpr CFIndex linkImageTitleMaximumLineCount = 2;

struct LinkImageLayout {
    LinkImageLayout(URL&, const String& title);

    struct Label {
        FloatPoint origin;
        RetainPtr<CTFrameRef> frame;
    };
    Vector<Label> labels;

    FloatRect boundingRect;
};

LinkImageLayout::LinkImageLayout(URL& url, const String& titleString)
{
    RetainPtr title = nsStringNilIfEmpty(titleString);
    RetainPtr nsURL = url.createNSURL();
    NSString *absoluteURLString = [nsURL absoluteString];

    RetainPtr domain = absoluteURLString;
#if HAVE(URL_FORMATTING)
    domain = [nsURL _lp_simplifiedDisplayString];
#else
    if (LinkPresentationLibrary())
        domain = [nsURL _lp_simplifiedDisplayString];
#endif

    if ([title isEqualToString:absoluteURLString])
        title = nil;

    NSFont *titleFont = [NSFont boldSystemFontOfSize:linkImageFontSize];
    NSFont *domainFont = [NSFont systemFontOfSize:linkImageFontSize];

    NSColor *titleColor = [NSColor labelColor];
    NSColor *domainColor = [NSColor secondaryLabelColor];

    CGFloat maximumAvailableWidth = linkImageMaximumWidth - linkImagePadding * 2;

    CGFloat currentY = linkImagePadding;
    CGFloat maximumUsedTextWidth = 0;

    auto buildLines = [this, maximumAvailableWidth, &maximumUsedTextWidth, &currentY](NSString *text, NSColor *color, NSFont *font, CFIndex maximumLines, CTLineBreakMode lineBreakMode) {
        CTParagraphStyleSetting paragraphStyleSettings;
        paragraphStyleSettings.spec = kCTParagraphStyleSpecifierLineBreakMode;
        paragraphStyleSettings.valueSize = sizeof(CTLineBreakMode);
        paragraphStyleSettings.value = &lineBreakMode;
        RetainPtr<CTParagraphStyleRef> paragraphStyle = adoptCF(CTParagraphStyleCreate(&paragraphStyleSettings, 1));

        NSDictionary *textAttributes = @{
            (id)kCTFontAttributeName: font,
            (id)kCTForegroundColorAttributeName: color,
            (id)kCTParagraphStyleAttributeName: (id)paragraphStyle.get()
        };
        NSDictionary *frameAttributes = @{
            (id)kCTFrameMaximumNumberOfLinesAttributeName: @(maximumLines)
        };

        RetainPtr<NSAttributedString> attributedText = adoptNS([[NSAttributedString alloc] initWithString:text attributes:textAttributes]);
        RetainPtr<CTFramesetterRef> textFramesetter = adoptCF(CTFramesetterCreateWithAttributedString((CFAttributedStringRef)attributedText.get()));

        CFRange fitRange;
        CGSize textSize = CTFramesetterSuggestFrameSizeWithConstraints(textFramesetter.get(), CFRangeMake(0, 0), (CFDictionaryRef)frameAttributes, CGSizeMake(maximumAvailableWidth, CGFLOAT_MAX), &fitRange);

        RetainPtr<CGPathRef> textPath = adoptCF(CGPathCreateWithRect(CGRectMake(0, 0, textSize.width, textSize.height), nullptr));
        RetainPtr<CTFrameRef> textFrame = adoptCF(CTFramesetterCreateFrame(textFramesetter.get(), fitRange, textPath.get(), (CFDictionaryRef)frameAttributes));

        RetainPtr ctLines = CTFrameGetLines(textFrame.get());
        CFIndex lineCount = CFArrayGetCount(ctLines);
        if (!lineCount)
            return;

        Vector<CGPoint> origins(lineCount);
        CGRect lineBounds;
        CGFloat height = 0;
        CTFrameGetLineOrigins(textFrame.get(), CFRangeMake(0, 0), origins.mutableSpan().data());
        for (CFIndex lineIndex = 0; lineIndex < lineCount; ++lineIndex) {
            RetainPtr line = (CTLineRef)CFArrayGetValueAtIndex(ctLines, lineIndex);

            lineBounds = CTLineGetBoundsWithOptions(line, 0);
            CGFloat trailingWhitespaceWidth = CTLineGetTrailingWhitespaceWidth(line);
            CGFloat lineWidthIgnoringTrailingWhitespace = lineBounds.size.width - trailingWhitespaceWidth;
            maximumUsedTextWidth = std::max(maximumUsedTextWidth, lineWidthIgnoringTrailingWhitespace);

            if (lineIndex)
                height += origins[lineIndex - 1].y - origins[lineIndex].y;
        }

        LinkImageLayout::Label label;
        label.frame = textFrame;
        label.origin = FloatPoint(linkImagePadding, currentY + origins[0].y);
        labels.append(label);

        currentY += height + lineBounds.size.height;
    };

    if (title)
        buildLines(title.get(), titleColor, titleFont, linkImageTitleMaximumLineCount, kCTLineBreakByTruncatingTail);

    if (title && domain)
        currentY += linkImageDomainBaselineToTitleBaseline - (domainFont.ascender - domainFont.descender);

    if (domain)
        buildLines(domain, domainColor, domainFont, 1, kCTLineBreakByTruncatingMiddle);

    currentY += linkImagePadding;

    boundingRect = FloatRect(0, 0, maximumUsedTextWidth + linkImagePadding * 2, currentY);

    // To work around blurry drag images on 1x displays, make the width and height a multiple of 2.
    // FIXME: remove this workaround when <rdar://problem/33059739> is fixed.
    boundingRect.setWidth((static_cast<int>(boundingRect.width()) / 2) * 2);
    boundingRect.setHeight((static_cast<int>(boundingRect.height() / 2) * 2));
}

std::pair<DragImageRef, RefPtr<TextIndicator>> DragController::dragImageAndTextIndicatorForLink(Element& element, URL& url, const String& title, float deviceScaleFactor)
{
    LinkImageLayout layout(url, title);

    LocalDefaultSystemAppearance localAppearance(protect(element.document())->useDarkAppearance(protect(element.computedStyle())));

    auto imageSize = layout.boundingRect.size();
    RetainPtr<NSImage> dragImage = adoptNS([[NSImage alloc] initWithSize:imageSize]);
    [dragImage _web_lockFocusWithDeviceScaleFactor:deviceScaleFactor];

    RetainPtr<CGContextRef> cgContext = [NSGraphicsContext currentContext].CGContext;

    RetainPtr backgroundPath = adoptCF(CGPathCreateWithRoundedRect(layout.boundingRect, linkImageCornerRadius, linkImageCornerRadius, nullptr));
    CGContextSetFillColorWithColor(cgContext.get(), cachedCGColor(colorFromCocoaColor([NSColor controlBackgroundColor])).get());
    CGContextAddPath(cgContext.get(), backgroundPath.get());
    CGContextFillPath(cgContext.get());
    CGContextSetTextMatrix(cgContext.get(), CGAffineTransformIdentity);

    for (const auto& label : layout.labels) {
        CGContextSaveGState(cgContext.get());
        CGContextTranslateCTM(cgContext.get(), label.origin.x(), layout.boundingRect.height() - label.origin.y() - linkImagePadding);
        CTFrameDraw(label.frame.get(), cgContext.get());
        CGContextRestoreGState(cgContext.get());
    }

    [dragImage unlockFocus];

    return  { dragImage, nullptr };
}

#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)

static RetainPtr<CGImageRef> cgImageFromTextIndicator(const TextIndicator& textIndicator)
{
    RefPtr image = textIndicator.contentImage();
    if (!image)
        return { };

    RefPtr nativeImage = image->nativeImage();
    if (!nativeImage)
        return { };

    return nativeImage->platformImage();
}

std::pair<DragImageRef, RefPtr<TextIndicator>> DragController::dragImageAndTextIndicatorForLink(Element& linkElement, URL&, const String&, float)
{
    constexpr OptionSet<TextIndicatorOption> defaultLinkIndicatorOptions {
        TextIndicatorOption::TightlyFitContent,
        TextIndicatorOption::RespectTextColor,
        TextIndicatorOption::UseBoundingRectAndPaintAllContentForComplexRanges,
        TextIndicatorOption::ExpandClipBeyondVisibleRect,
        TextIndicatorOption::ComputeEstimatedBackgroundColor
    };

    RefPtr textIndicator = TextIndicator::createWithRange(makeRangeSelectingNodeContents(linkElement), defaultLinkIndicatorOptions, TextIndicatorPresentationTransition::None, { });
    if (!textIndicator)
        return  { nullptr, nullptr };

    return  { cgImageFromTextIndicator(*textIndicator).autorelease(), textIndicator };
}

constexpr OptionSet<TextIndicatorOption> defaultSelectionDragImageTextIndicatorOptions {
    TextIndicatorOption::ExpandClipBeyondVisibleRect,
    TextIndicatorOption::PaintAllContent,
    TextIndicatorOption::UseSelectionRectForSizing,
    TextIndicatorOption::ComputeEstimatedBackgroundColor
};

std::pair<DragImageRef, RefPtr<TextIndicator>> DragController::dragImageAndTextIndicatorForSelection(LocalFrame& frame, bool forceBlackText)
{
    if (RefPtr document = frame.document())
        document->updateLayout();

    auto options = defaultSelectionDragImageTextIndicatorOptions;
    if (!forceBlackText)
        options.add(TextIndicatorOption::RespectTextColor);

    RefPtr textIndicator = TextIndicator::createWithSelectionInFrame(frame, options, TextIndicatorPresentationTransition::None, FloatSize());
    if (!textIndicator)
        return { nullptr, nullptr };

    return { cgImageFromTextIndicator(*textIndicator).autorelease(), textIndicator };
}

DragImageRef DragController::dragImageForRange(LocalFrame& frame, const SimpleRange& range, bool forceBlackText)
{
    if (RefPtr document = frame.document())
        document->updateLayout();

    if (range.collapsed())
        return nil;

    auto options = defaultSelectionDragImageTextIndicatorOptions;
    if (!forceBlackText)
        options.add(TextIndicatorOption::RespectTextColor);

    auto textIndicator = TextIndicator::createWithRange(range, options, TextIndicatorPresentationTransition::None);
    if (!textIndicator || !textIndicator->contentImage())
        return nil;

    Ref image = *textIndicator->contentImage();
    RetainPtr render = adoptNS([PAL::allocUIGraphicsImageRendererInstance() initWithSize:image->size()]);
    RetainPtr finalImage = [render imageWithActions:[&image](UIGraphicsImageRendererContext *rendererContext) {
        GraphicsContextCG context(protect(rendererContext.CGContext).get());
        context.drawBitmapImage(image, FloatPoint());
    }];

    return [finalImage CGImage];
}

#endif // PLATFORM(IOS_FAMILY)

} // namespace WebCore

#endif // ENABLE(DRAG_SUPPORT)
