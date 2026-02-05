/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#import "WKDOMRangePrivate.h"

#import "InjectedBundleRangeHandle.h"
#import "WKBundleAPICast.h"
#import "WKDOMInternals.h"
#import <WebCore/BoundaryPointInlines.h>
#import <WebCore/Document.h>
#import <WebCore/TextIterator.h>
#import <WebCore/VisibleUnits.h>
#import <wtf/MainThread.h>
#import <wtf/cocoa/VectorCocoa.h>

static Ref<WebCore::Range> protectedImpl(WKDOMRange *range)
{
    return *range->_impl;
}

@implementation WKDOMRange

- (id)_initWithImpl:(WebCore::Range*)impl
{
    self = [super init];
    if (!self)
        return nil;

    _impl = impl;
    WebKit::WKDOMRangeCache().add(impl, self);

    return self;
}

- (id)initWithDocument:(WKDOMDocument *)document
{
    return [self _initWithImpl:WebCore::Range::create(*WebKit::toProtectedWebCoreDocument(document)).ptr()];
}

- (void)dealloc
{
    ensureOnMainRunLoop([range = std::exchange(_impl, nullptr)] {
        WebKit::WKDOMRangeCache().remove(range.get());
    });
    [super dealloc];
}

- (void)setStart:(WKDOMNode *)node offset:(int)offset
{
    if (!node)
        return;
    protectedImpl(self)->setStart(*WebKit::toWebCoreNode(node), offset);
}

- (void)setEnd:(WKDOMNode *)node offset:(int)offset
{
    if (!node)
        return;
    protectedImpl(self)->setEnd(*WebKit::toWebCoreNode(node), offset);
}

- (void)collapse:(BOOL)toStart
{
    protectedImpl(self)->collapse(toStart);
}

- (void)selectNode:(WKDOMNode *)node
{
    if (!node)
        return;
    protectedImpl(self)->selectNode(*WebKit::toProtectedWebCoreNode(node));
}

- (void)selectNodeContents:(WKDOMNode *)node
{
    if (!node)
        return;
    protectedImpl(self)->selectNodeContents(*WebKit::toProtectedWebCoreNode(node));
}

- (WKDOMNode *)startContainer
{
    return WebKit::toWKDOMNode(protect(protectedImpl(self)->startContainer()).ptr());
}

- (NSInteger)startOffset
{
    return protectedImpl(self)->startOffset();
}

- (WKDOMNode *)endContainer
{
    return WebKit::toWKDOMNode(protect(protectedImpl(self)->endContainer()).ptr());
}

- (NSInteger)endOffset
{
    return protectedImpl(self)->endOffset();
}

- (NSString *)text
{
    auto range = makeSimpleRange(protectedImpl(self));
    protect(range.start.document())->updateLayout();
    return plainText(range).createNSString().autorelease();
}

- (BOOL)isCollapsed
{
    return protectedImpl(self)->collapsed();
}

- (NSArray *)textRects
{
    auto range = makeSimpleRange(protectedImpl(self));
    protect(range.start.document())->updateLayout(WebCore::LayoutOptions::IgnorePendingStylesheets);
    return createNSArray(WebCore::RenderObject::absoluteTextRects(range)).autorelease();
}

- (WKDOMRange *)rangeByExpandingToWordBoundaryByCharacters:(NSUInteger)characters inDirection:(WKDOMRangeDirection)direction
{
    auto range = makeSimpleRange(protectedImpl(self));
    auto newRange = rangeExpandedByCharactersInDirectionAtWordBoundary(makeDeprecatedLegacyPosition(direction == WKDOMRangeDirectionForward ? range.end : range.start), characters, direction == WKDOMRangeDirectionForward ? WebCore::SelectionDirection::Forward : WebCore::SelectionDirection::Backward);
    return adoptNS([[WKDOMRange alloc] _initWithImpl:createLiveRange(newRange).get()]).autorelease();
}

@end

@implementation WKDOMRange (WKPrivate)

- (WKBundleRangeHandleRef)_copyBundleRangeHandleRef
{
    auto rangeHandle = WebKit::InjectedBundleRangeHandle::getOrCreate(protectedImpl(self).ptr());
    return toAPILeakingRef(WTF::move(rangeHandle));
}

@end
