/*
 * Copyright (C) 2014-2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "DictionaryLookup.h"

#if PLATFORM(COCOA)

#if ENABLE(REVEAL)

#import "Document.h"
#import "Editing.h"
#import "FocusController.h"
#import "Frame.h"
#import "FrameSelection.h"
#import "GraphicsContextCG.h"
#import "HTMLConverter.h"
#import "HitTestResult.h"
#import "NotImplemented.h"
#import "Page.h"
#import "Range.h"
#import "RenderObject.h"
#import "TextIterator.h"
#import "VisiblePosition.h"
#import "VisibleSelection.h"
#import "VisibleUnits.h"
#import <pal/ios/UIKitSoftLink.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/RevealSPI.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <pal/spi/mac/LookupSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/RefPtr.h>

#if !PLATFORM(WATCH)
#import <PDFKit/PDFKit.h>
#endif // !PLATFORM(WATCH)

#if PLATFORM(IOSMAC)
extern "C" {
#import <UIKitMacHelper/UINSRevealController.h>
}
SOFT_LINK_PRIVATE_FRAMEWORK(UIKitMacHelper)
SOFT_LINK(UIKitMacHelper, UINSSharedRevealController, id<UINSRevealController>, (void), ())

#endif // PLATFORM(IOSMAC)

#if PLATFORM(MAC)

@interface WebRevealHighlight <RVPresenterHighlightDelegate> : NSObject {
@private
    Function<void()> _clearTextIndicator;
    NSRect _highlightRect;
    BOOL _useDefaultHighlight;
    NSAttributedString *_attributedString;
}

@property (nonatomic, readonly) NSRect highlightRect;
@property (nonatomic, readonly) BOOL useDefaultHighlight;
@property (nonatomic, readonly) NSAttributedString *attributedString;

- (instancetype)initWithHighlightRect:(NSRect)highlightRect useDefaultHighlight:(BOOL)useDefaultHighlight attributedString:(NSAttributedString *) attributedString;
- (void)setClearTextIndicator:(Function<void()>&&)clearTextIndicator;

@end

@implementation WebRevealHighlight

@synthesize highlightRect=_highlightRect;
@synthesize useDefaultHighlight=_useDefaultHighlight;
@synthesize attributedString=_attributedString;

- (instancetype)initWithHighlightRect:(NSRect)highlightRect useDefaultHighlight:(BOOL)useDefaultHighlight attributedString:(NSAttributedString *) attributedString
{
    if (!(self = [super init]))
        return nil;
    
    _highlightRect = highlightRect;
    _useDefaultHighlight = useDefaultHighlight;
    _attributedString = attributedString;
    
    return self;
}

- (void)setClearTextIndicator:(Function<void()>&&)clearTextIndicator
{
    _clearTextIndicator = WTFMove(clearTextIndicator);
}

- (NSArray<NSValue *> *)revealContext:(RVPresentingContext *)context rectsForItem:(RVItem *)item
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(item);
    return @[[NSValue valueWithRect:self.highlightRect]];
}

- (void)revealContext:(RVPresentingContext *)context drawRectsForItem:(RVItem *)item
{
    UNUSED_PARAM(item);
    
    for (NSValue *rectVal in context.itemRectsInView) {
        NSRect rect = rectVal.rectValue;

        // Get current font attributes from the attributed string above, and add paragraph style attribute in order to center text.
        RetainPtr<NSMutableDictionary> attributes = adoptNS([[NSMutableDictionary alloc] initWithDictionary:[self.attributedString fontAttributesInRange:NSMakeRange(0, [self.attributedString length])]]);
        RetainPtr<NSMutableParagraphStyle> paragraph = adoptNS([[NSMutableParagraphStyle alloc] init]);
        [paragraph setAlignment:NSTextAlignmentCenter];
        [attributes setObject:paragraph.get() forKey:NSParagraphStyleAttributeName];
    
        RetainPtr<NSAttributedString> string = adoptNS([[NSAttributedString alloc] initWithString:[self.attributedString string] attributes:attributes.get()]);
        [string drawInRect:rect];
    }
}

- (BOOL)revealContext:(RVPresentingContext *)context shouldUseDefaultHighlightForItem:(RVItem *)item
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(item);
    return self.useDefaultHighlight;
}

- (void)revealContext:(RVPresentingContext *)context stopHighlightingItem:(RVItem *)item
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(item);
    auto block = WTFMove(_clearTextIndicator);
    if (block)
        block();
}

@end

#elif PLATFORM(IOSMAC) // PLATFORM(MAC)

@interface WebRevealHighlight <UIRVPresenterHighlightDelegate> : NSObject {
@private
    RefPtr<WebCore::Image> _image;
    CGRect _highlightRect;
    BOOL _highlighting;
    UIView *_view;
}

- (instancetype)initWithHighlightRect:(NSRect)highlightRect view:(UIView *)view image:(RefPtr<WebCore::Image>&&)image;

@end

@implementation WebRevealHighlight

- (instancetype)initWithHighlightRect:(NSRect)highlightRect view:(UIView *)view image:(RefPtr<WebCore::Image>&&)image
{
    if (!(self = [super init]))
        return nil;
    
    _highlightRect = highlightRect;
    _view = view;
    _highlighting = NO;
    _image = image;
    
    return self;
}

- (void)setImage:(RefPtr<WebCore::Image>&&)image
{
    _image = WTFMove(image);
}

- (NSArray<NSValue *> *)highlightRectsForItem:(RVItem *)item
{
    UNUSED_PARAM(item);
    return @[[NSValue valueWithCGRect:_highlightRect]];
}

- (void)startHighlightingItem:(RVItem *)item
{
    UNUSED_PARAM(item);
    _highlighting = YES;
}

- (void)highlightItem:(RVItem *)item withProgress:(CGFloat)progress
{
    UNUSED_PARAM(item);
    UNUSED_PARAM(progress);
}

- (void)completeHighlightingItem:(RVItem *)item
{
    UNUSED_PARAM(item);
}

- (void)stopHighlightingItem:(RVItem *)item
{
    UNUSED_PARAM(item);
    _highlighting = NO;
}

- (void)highlightRangeChangedForItem:(RVItem *)item
{
    UNUSED_PARAM(item);
}

- (BOOL)highlighting
{
    return _highlighting;
}

- (void)drawHighlightContentForItem:(RVItem *)item context:(CGContextRef)context
{
    NSArray <NSValue *> *rects = [self highlightRectsForItem:item];
    if (!rects.count)
        return;
    
    CGRect highlightRect = rects.firstObject.CGRectValue;
    for (NSValue *rect in rects)
        highlightRect = CGRectUnion(highlightRect, rect.CGRectValue);
    highlightRect = [_view convertRect:highlightRect fromView:nil];
    
    WebCore::CGContextStateSaver saveState(context);
    CGAffineTransform contextTransform = CGContextGetCTM(context);
    CGFloat backingScale = contextTransform.a;
    CGFloat iOSMacScaleFactor = [getUIApplicationClass() sharedApplication]._iOSMacScale;
    CGAffineTransform transform = CGAffineTransformMakeScale(iOSMacScaleFactor * backingScale, iOSMacScaleFactor * backingScale);
    CGContextSetCTM(context, transform);
    
    for (NSValue *v in rects) {
        CGRect imgSrcRect = [_view convertRect:v.CGRectValue fromView:nil];
        RetainPtr<CGImageRef> imageRef = _image->nativeImage();
        CGRect origin = CGRectMake(imgSrcRect.origin.x - highlightRect.origin.x, imgSrcRect.origin.y - highlightRect.origin.y, highlightRect.size.width, highlightRect.size.height);
        CGContextDrawImage(context, origin, imageRef.get());
    }
}

@end

#endif // PLATFORM(IOSMAC)

#endif // ENABLE(REVEAL)

namespace WebCore {

#if ENABLE(REVEAL)

std::tuple<RefPtr<Range>, NSDictionary *> DictionaryLookup::rangeForSelection(const VisibleSelection& selection)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    if (!getRVItemClass())
        return { nullptr, nil };
    
    auto selectedRange = selection.toNormalizedRange();
    if (!selectedRange)
        return { nullptr, nil };

    // Since we already have the range we want, we just need to grab the returned options.
    auto selectionStart = selection.visibleStart();
    auto selectionEnd = selection.visibleEnd();

    // As context, we are going to use the surrounding paragraphs of text.
    auto paragraphStart = startOfParagraph(selectionStart);
    auto paragraphEnd = endOfParagraph(selectionEnd);

    int lengthToSelectionStart = TextIterator::rangeLength(makeRange(paragraphStart, selectionStart).get());
    int lengthToSelectionEnd = TextIterator::rangeLength(makeRange(paragraphStart, selectionEnd).get());
    NSRange rangeToPass = NSMakeRange(lengthToSelectionStart, lengthToSelectionEnd - lengthToSelectionStart);
    
    RefPtr<Range> fullCharacterRange = makeRange(paragraphStart, paragraphEnd);
    String itemString = plainText(fullCharacterRange.get());
    RetainPtr<RVItem> item = adoptNS([allocRVItemInstance() initWithText:itemString selectedRange:rangeToPass]);
    NSRange highlightRange = item.get().highlightRange;
    
    return { TextIterator::subrange(*fullCharacterRange, highlightRange.location, highlightRange.length), nil };
    
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return { nullptr, nil };
}

std::tuple<RefPtr<Range>, NSDictionary *> DictionaryLookup::rangeAtHitTestResult(const HitTestResult& hitTestResult)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    if (!getRVItemClass())
        return { nullptr, nil };
    
    auto* node = hitTestResult.innerNonSharedNode();
    if (!node || !node->renderer())
        return { nullptr, nil };

    auto* frame = node->document().frame();
    if (!frame)
        return { nullptr, nil };

    // Don't do anything if there is no character at the point.
    auto framePoint = hitTestResult.roundedPointInInnerNodeFrame();
    if (!frame->rangeForPoint(framePoint))
        return { nullptr, nil };

    auto position = frame->visiblePositionForPoint(framePoint);
    if (position.isNull())
        position = firstPositionInOrBeforeNode(node);

    auto selection = frame->page()->focusController().focusedOrMainFrame().selection().selection();
    NSRange selectionRange;
    int hitIndex;
    RefPtr<Range> fullCharacterRange;
    
    if (selection.selectionType() == VisibleSelection::RangeSelection) {
        auto selectionStart = selection.visibleStart();
        auto selectionEnd = selection.visibleEnd();
        
        // As context, we are going to use the surrounding paragraphs of text.
        auto paragraphStart = startOfParagraph(selectionStart);
        auto paragraphEnd = endOfParagraph(selectionEnd);
        
        auto rangeToSelectionStart = makeRange(paragraphStart, selectionStart);
        auto rangeToSelectionEnd = makeRange(paragraphStart, selectionEnd);
        
        fullCharacterRange = makeRange(paragraphStart, paragraphEnd);
        
        selectionRange = NSMakeRange(TextIterator::rangeLength(rangeToSelectionStart.get()), TextIterator::rangeLength(makeRange(selectionStart, selectionEnd).get()));
        
        hitIndex = TextIterator::rangeLength(makeRange(paragraphStart, position).get());
    } else {
        VisibleSelection selectionAccountingForLineRules { position };
        selectionAccountingForLineRules.expandUsingGranularity(WordGranularity);
        position = selectionAccountingForLineRules.start();
        // As context, we are going to use 250 characters of text before and after the point.
        fullCharacterRange = rangeExpandedAroundPositionByCharacters(position, 250);
        
        if (!fullCharacterRange)
            return { nullptr, nil };
        
        selectionRange = NSMakeRange(NSNotFound, 0);
        hitIndex = TextIterator::rangeLength(makeRange(fullCharacterRange->startPosition(), position).get());
    }
    
    NSRange selectedRange = [getRVSelectionClass() revealRangeAtIndex:hitIndex selectedRanges:@[[NSValue valueWithRange:selectionRange]] shouldUpdateSelection:nil];
    
    String itemString = plainText(fullCharacterRange.get());
    RetainPtr<RVItem> item = adoptNS([allocRVItemInstance() initWithText:itemString selectedRange:selectedRange]);
    NSRange highlightRange = item.get().highlightRange;

    if (highlightRange.location == NSNotFound || !highlightRange.length)
        return { nullptr, nil };
    
    return { TextIterator::subrange(*fullCharacterRange, highlightRange.location, highlightRange.length), nil };
    
    END_BLOCK_OBJC_EXCEPTIONS;
    
    return { nullptr, nil };
    
}
    
#if !PLATFORM(WATCH)

static void expandSelectionByCharacters(PDFSelection *selection, NSInteger numberOfCharactersToExpand, NSInteger& charactersAddedBeforeStart, NSInteger& charactersAddedAfterEnd)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    size_t originalLength = selection.string.length;
    [selection extendSelectionAtStart:numberOfCharactersToExpand];
    
    charactersAddedBeforeStart = selection.string.length - originalLength;
    
    [selection extendSelectionAtEnd:numberOfCharactersToExpand];
    charactersAddedAfterEnd = selection.string.length - originalLength - charactersAddedBeforeStart;

    END_BLOCK_OBJC_EXCEPTIONS;
}

std::tuple<NSString *, NSDictionary *> DictionaryLookup::stringForPDFSelection(PDFSelection *selection)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
    if (!getRVItemClass())
        return { nullptr, nil };

    // Don't do anything if there is no character at the point.
    if (!selection || !selection.string.length)
        return { @"", nil };

    RetainPtr<PDFSelection> selectionForLookup = adoptNS([selection copy]);

    // As context, we are going to use 250 characters of text before and after the point.
    auto originalLength = [selectionForLookup string].length;
    NSInteger charactersAddedBeforeStart = 0;
    NSInteger charactersAddedAfterEnd = 0;
    expandSelectionByCharacters(selectionForLookup.get(), 250, charactersAddedBeforeStart, charactersAddedAfterEnd);

    auto fullPlainTextString = [selectionForLookup string];
    auto rangeToPass = NSMakeRange(charactersAddedBeforeStart, 0);

    RetainPtr<RVItem> item = adoptNS([allocRVItemInstance() initWithText:fullPlainTextString selectedRange:rangeToPass]);
    NSRange extractedRange = item.get().highlightRange;
    
    if (extractedRange.location == NSNotFound)
        return { selection.string, nil };

    NSInteger lookupAddedBefore = rangeToPass.location - extractedRange.location;
    NSInteger lookupAddedAfter = (extractedRange.location + extractedRange.length) - (rangeToPass.location + originalLength);

    [selection extendSelectionAtStart:lookupAddedBefore];
    [selection extendSelectionAtEnd:lookupAddedAfter];

    ASSERT([selection.string isEqualToString:[fullPlainTextString substringWithRange:extractedRange]]);
    return { selection.string, nil };

    END_BLOCK_OBJC_EXCEPTIONS;

    return { @"", nil };
}
    
#endif // !PLATFORM(WATCH)

static WKRevealController showPopupOrCreateAnimationController(bool createAnimationController, const DictionaryPopupInfo& dictionaryPopupInfo, RevealView *view, const WTF::Function<void(TextIndicator&)>& textIndicatorInstallationCallback, const WTF::Function<FloatRect(FloatRect)>& rootViewToViewConversionCallback, WTF::Function<void()>&& clearTextIndicator)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    
#if PLATFORM(MAC)
    
    if (!getRVItemClass() || !getRVPresenterClass())
        return nil;

    RetainPtr<NSMutableDictionary> mutableOptions = adoptNS([[NSMutableDictionary alloc] init]);
    if (NSDictionary *options = dictionaryPopupInfo.options.get())
        [mutableOptions addEntriesFromDictionary:options];

    auto textIndicator = TextIndicator::create(dictionaryPopupInfo.textIndicator);
    
    RetainPtr<RVPresenter> presenter = adoptNS([allocRVPresenterInstance() init]);
    
    NSRect highlightRect;
    NSPoint pointerLocation;
    
    if (textIndicator.get().contentImage()) {
        textIndicatorInstallationCallback(textIndicator.get());

        FloatRect firstTextRectInViewCoordinates = textIndicator.get().textRectsInBoundingRectCoordinates()[0];
        FloatRect textBoundingRectInViewCoordinates = textIndicator.get().textBoundingRectInRootViewCoordinates();
        FloatRect selectionBoundingRectInViewCoordinates = textIndicator.get().selectionRectInRootViewCoordinates();
        
        if (rootViewToViewConversionCallback) {
            textBoundingRectInViewCoordinates = rootViewToViewConversionCallback(textBoundingRectInViewCoordinates);
            selectionBoundingRectInViewCoordinates = rootViewToViewConversionCallback(selectionBoundingRectInViewCoordinates);
        }
        
        firstTextRectInViewCoordinates.moveBy(textBoundingRectInViewCoordinates.location());
        highlightRect = selectionBoundingRectInViewCoordinates;
        pointerLocation = firstTextRectInViewCoordinates.location();
        
    } else {
        NSPoint textBaselineOrigin = dictionaryPopupInfo.origin;
        
        highlightRect = textIndicator->selectionRectInRootViewCoordinates();
        pointerLocation = [view convertPoint:textBaselineOrigin toView:nil];
    }
    
    RetainPtr<WebRevealHighlight> webHighlight =  adoptNS([[WebRevealHighlight alloc] initWithHighlightRect: highlightRect useDefaultHighlight:!textIndicator.get().contentImage() attributedString:dictionaryPopupInfo.attributedString.get()]);
    RetainPtr<RVPresentingContext> context = adoptNS([allocRVPresentingContextInstance() initWithPointerLocationInView:pointerLocation inView:view highlightDelegate:(id<RVPresenterHighlightDelegate>) webHighlight.get()]);
    
    RetainPtr<RVItem> item = adoptNS([allocRVItemInstance() initWithText:dictionaryPopupInfo.attributedString.get().string selectedRange:NSMakeRange(0, 0)]);
    
    [webHighlight setClearTextIndicator:[webHighlight = WTFMove(webHighlight), clearTextIndicator = WTFMove(clearTextIndicator)] {
        if (clearTextIndicator)
            clearTextIndicator();
    }];
    
    if (createAnimationController)
        return [presenter animationControllerForItem:item.get() documentContext:nil presentingContext:context.get() options:nil];
    [presenter revealItem:item.get() documentContext:nil presentingContext:context.get() options:nil];
    return nil;
    
#elif PLATFORM(IOSMAC)
    
    UNUSED_PARAM(textIndicatorInstallationCallback);
    UNUSED_PARAM(rootViewToViewConversionCallback);
    UNUSED_PARAM(clearTextIndicator);
    ASSERT_UNUSED(createAnimationController, !createAnimationController);

    auto textIndicator = TextIndicator::create(dictionaryPopupInfo.textIndicator);
    
    RetainPtr<WebRevealHighlight> webHighlight = adoptNS([[WebRevealHighlight alloc] initWithHighlightRect:[view convertRect:textIndicator->selectionRectInRootViewCoordinates() toView:nil] view:view image:textIndicator->contentImage()]);
    
    RetainPtr<RVItem> item = adoptNS([allocRVItemInstance() initWithText:dictionaryPopupInfo.attributedString.get().string selectedRange:NSMakeRange(0, 0)]);
    
    [UINSSharedRevealController() revealItem:item.get() locationInWindow:dictionaryPopupInfo.origin window:view.window highlighter:(id<UIRVPresenterHighlightDelegate>) webHighlight.get()];
    return nil;
    
#else // PLATFORM(IOS_FAMILY)
    
    UNUSED_PARAM(createAnimationController);
    UNUSED_PARAM(dictionaryPopupInfo);
    UNUSED_PARAM(view);
    UNUSED_PARAM(textIndicatorInstallationCallback);
    UNUSED_PARAM(rootViewToViewConversionCallback);
    UNUSED_PARAM(clearTextIndicator);
    
    return nil;
#endif // PLATFORM(IOS_FAMILY)
    
    END_BLOCK_OBJC_EXCEPTIONS;
    
}

void DictionaryLookup::showPopup(const DictionaryPopupInfo& dictionaryPopupInfo, RevealView *view, const WTF::Function<void(TextIndicator&)>& textIndicatorInstallationCallback, const WTF::Function<FloatRect(FloatRect)>& rootViewToViewConversionCallback, WTF::Function<void()>&& clearTextIndicator)
{
    showPopupOrCreateAnimationController(false, dictionaryPopupInfo, view, textIndicatorInstallationCallback, rootViewToViewConversionCallback, WTFMove(clearTextIndicator));
}

void DictionaryLookup::hidePopup()
{
    notImplemented();
}

#if PLATFORM(MAC)

WKRevealController DictionaryLookup::animationControllerForPopup(const DictionaryPopupInfo& dictionaryPopupInfo, NSView *view, const WTF::Function<void(TextIndicator&)>& textIndicatorInstallationCallback, const WTF::Function<FloatRect(FloatRect)>& rootViewToViewConversionCallback, WTF::Function<void()>&& clearTextIndicator)
{
    return showPopupOrCreateAnimationController(true, dictionaryPopupInfo, view, textIndicatorInstallationCallback, rootViewToViewConversionCallback, WTFMove(clearTextIndicator));
}

#endif // PLATFORM(MAC)

#elif PLATFORM(IOS_FAMILY) // ENABLE(REVEAL)

std::tuple<RefPtr<Range>, NSDictionary *> DictionaryLookup::rangeForSelection(const VisibleSelection&)
{
    return { nullptr, nil };
}

std::tuple<RefPtr<Range>, NSDictionary *> DictionaryLookup::rangeAtHitTestResult(const HitTestResult&)
{
    return { nullptr, nil };
}

#endif // ENABLE(REVEAL)

} // namespace WebCore

#endif // PLATFORM(COCOA)
