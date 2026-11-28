/*
 * Copyright (C) 2012-2026 Apple Inc. All rights reserved.
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
#import "WKTextRange.h"

#if PLATFORM(IOS_FAMILY)

#import "WebAutocorrectionContext.h"
#import <wtf/cocoa/TypeCastsCocoa.h>

#if USE(BROWSERENGINEKIT)

@implementation WKUITextSelectionRect {
    CGRect _selectionRect;
}

+ (instancetype)selectionRectWithCGRect:(CGRect)rect
{
    return adoptNS([[WKUITextSelectionRect alloc] initWithCGRect:rect]).autorelease();
}

- (instancetype)initWithCGRect:(CGRect)rect
{
    if (self = [super init])
        _selectionRect = rect;
    return self;
}

- (CGRect)rect
{
    return _selectionRect;
}

@end

#endif // USE(BROWSERENGINEKIT)

inline static std::pair<OptionSet<WebKit::TextPositionAnchor>, NSInteger> anchorsAndOffset(UITextPosition *position)
{
    if (auto concretePosition = dynamic_objc_cast<WKTextPosition>(position); concretePosition.anchors)
        return { concretePosition.anchors, 0 };

    if (auto relativePosition = dynamic_objc_cast<WKRelativeTextPosition>(position))
        return { relativePosition.anchors, relativePosition.offset };

    return { { }, 0 };
}

@implementation WKRelativeTextPosition

- (instancetype)initWithAnchors:(OptionSet<WebKit::TextPositionAnchor>)anchors offset:(NSInteger)offset
{
    if (self = [super init]) {
        _anchors = anchors;
        _offset = offset;
    }
    return self;
}

- (BOOL)isRelativeToStart
{
    return _anchors.contains(WebKit::TextPositionAnchor::Start);
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"WKRelativeTextPosition(%s, %d)", _anchors.contains(WebKit::TextPositionAnchor::Start) ? "start" : "end", static_cast<int>(_offset)];
}

@end

@implementation WKRelativeTextRange {
    RetainPtr<WKRelativeTextPosition> _start;
    RetainPtr<WKRelativeTextPosition> _end;
}

- (instancetype)initWithStart:(UITextPosition *)start end:(UITextPosition *)end
{
    if (self = [super init]) {
        auto [startAnchors, startOffset] = anchorsAndOffset(start);
        auto [endAnchors, endOffset] = anchorsAndOffset(end);
        if (!startAnchors.isEmpty() && !endAnchors.isEmpty()) {
            _start = adoptNS([[WKRelativeTextPosition alloc] initWithAnchors:startAnchors offset:startOffset]);
            _end = adoptNS([[WKRelativeTextPosition alloc] initWithAnchors:endAnchors offset:endOffset]);
        }
    }
    return self;
}

- (WKRelativeTextPosition *)start
{
    return _start.get();
}

- (WKRelativeTextPosition *)end
{
    return _end.get();
}

- (BOOL)isEmpty
{
    return [_start anchors] == [_end anchors] && [_start offset] == [_end offset];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"WKRelativeTextRange(start=%@, end=%@)", _start.get(), _end.get()];
}

@end

@implementation WKTextRange (UITextInputAdditions)

- (BOOL)_isCaret
{
    return self.empty;
}

- (BOOL)_isRanged
{
    return !self.empty;
}

@end

@implementation WKTextRange

+ (WKTextRange *)textRangeWithState:(BOOL)isNone isRange:(BOOL)isRange isEditable:(BOOL)isEditable startRect:(CGRect)startRect endRect:(CGRect)endRect selectionRects:(NSArray *)selectionRects selectedTextLength:(NSUInteger)selectedTextLength
{
    RetainPtr range = adoptNS([[WKTextRange alloc] init]);
    [range setIsNone:isNone];
    [range setIsRange:isRange];
    [range setIsEditable:isEditable];
    [range setStartRect:startRect];
    [range setEndRect:endRect];
    [range setSelectedTextLength:selectedTextLength];
    [range setSelectionRects:selectionRects];
    return range.autorelease();
}

- (void)dealloc
{
    [super dealloc];
}

- (NSArray<WKTextSelectionRect *> *)selectionRects
{
    return _selectionRects.get();
}

- (void)setSelectionRects:(NSArray<WKTextSelectionRect *> *)selectionRects
{
    _selectionRects = adoptNS([selectionRects copy]);
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"%@(%p) - start:%@, end:%@", [self class], self, protect(NSStringFromCGRect(self.startRect)).get(), protect(NSStringFromCGRect(self.endRect)).get()];
}

- (WKTextPosition *)start
{
    WKTextPosition *position = [WKTextPosition textPositionWithRect:self.startRect];
    OptionSet anchors { WebKit::TextPositionAnchor::Start };
    if (self.isEmpty)
        anchors.add(WebKit::TextPositionAnchor::End);
    position.anchors = anchors;
    return position;
}

- (WKTextPosition *)end
{
    WKTextPosition *position = [WKTextPosition textPositionWithRect:self.endRect];
    OptionSet anchors { WebKit::TextPositionAnchor::End };
    if (self.isEmpty)
        anchors.add(WebKit::TextPositionAnchor::Start);
    position.anchors = anchors;
    return position;
}

- (BOOL)isEmpty
{
    return !self.isRange;
}

// FIXME: Overriding isEqual: without overriding hash will cause trouble if this ever goes into an NSSet or is the key in an NSDictionary,
// since two equal items could have different hashes.
- (BOOL)isEqual:(id)other
{
    if (![other isKindOfClass:[WKTextRange class]])
        return NO;

    WKTextRange *otherRange = (WKTextRange *)other;

    if (self == other)
        return YES;

    // FIXME: Probably incorrect for equality to ignore so much of the object state.
    // It ignores isNone, isEditable, selectedTextLength, and selectionRects.

    if (self.isRange) {
        if (!otherRange.isRange)
            return NO;
        return CGRectEqualToRect(self.startRect, otherRange.startRect) && CGRectEqualToRect(self.endRect, otherRange.endRect);
    }

    if (otherRange.isRange)
        return NO;
    // FIXME: Do we need to check isNone here?
    return CGRectEqualToRect(self.startRect, otherRange.startRect);
}

@end

@implementation WKTextPosition

@synthesize positionRect = _positionRect;

+ (WKTextPosition *)textPositionWithRect:(CGRect)positionRect
{
    RetainPtr pos = adoptNS([[WKTextPosition alloc] init]);
    [pos setPositionRect:positionRect];
    return pos.autorelease();
}

// FIXME: Overriding isEqual: without overriding hash will cause trouble if this ever goes into a NSSet or is the key in an NSDictionary,
// since two equal items could have different hashes.
- (BOOL)isEqual:(id)other
{
    if (![other isKindOfClass:[WKTextPosition class]])
        return NO;

    return CGRectEqualToRect(self.positionRect, ((WKTextPosition *)other).positionRect);
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<WKTextPosition: %p, {%@}>", self, protect(NSStringFromCGRect(self.positionRect)).get()];
}

@end

@implementation WKAutocorrectionRects

+ (WKAutocorrectionRects *)autocorrectionRectsWithFirstCGRect:(CGRect)firstRect lastCGRect:(CGRect)lastRect
{
    RetainPtr rects = adoptNS([[WKAutocorrectionRects alloc] init]);
    [rects setFirstRect:firstRect];
    [rects setLastRect:lastRect];
    return rects.autorelease();
}

@end

@implementation WKAutocorrectionContext

+ (WKAutocorrectionContext *)emptyAutocorrectionContext
{
    return [self autocorrectionContextWithWebContext:WebKit::WebAutocorrectionContext { }];
}

+ (WKAutocorrectionContext *)autocorrectionContextWithWebContext:(const WebKit::WebAutocorrectionContext&)webCorrection
{
    RetainPtr correction = adoptNS([[WKAutocorrectionContext alloc] init]);
    [correction setContextBeforeSelection:nsStringNilIfEmpty(webCorrection.contextBefore).get()];
    [correction setSelectedText:nsStringNilIfEmpty(webCorrection.selectedText).get()];
    [correction setMarkedText:nsStringNilIfEmpty(webCorrection.markedText).get()];
    [correction setContextAfterSelection:nsStringNilIfEmpty(webCorrection.contextAfter).get()];
    [correction setRangeInMarkedText:webCorrection.selectedRangeInMarkedText];
    return correction.autorelease();
}

@end

#endif // PLATFORM(IOS_FAMILY)
