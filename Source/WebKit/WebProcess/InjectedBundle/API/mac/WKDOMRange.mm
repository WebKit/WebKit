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
#import <WebCore/Document.h>
#import <WebCore/VisibleUnits.h>
#import <wtf/cocoa/VectorCocoa.h>

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
    auto range = WebCore::Range::create(*WebKit::toWebCoreDocument(document));
    self = [self _initWithImpl:range.ptr()];
    if (!self)
        return nil;

    return self;
}

- (void)dealloc
{
    WebKit::WKDOMRangeCache().remove(_impl.get());
    [super dealloc];
}

- (void)setStart:(WKDOMNode *)node offset:(int)offset
{
    if (!node)
        return;
    _impl->setStart(*WebKit::toWebCoreNode(node), offset);
}

- (void)setEnd:(WKDOMNode *)node offset:(int)offset
{
    if (!node)
        return;
    _impl->setEnd(*WebKit::toWebCoreNode(node), offset);
}

- (void)collapse:(BOOL)toStart
{
    _impl->collapse(toStart);
}

- (void)selectNode:(WKDOMNode *)node
{
    if (!node)
        return;
    _impl->selectNode(*WebKit::toWebCoreNode(node));
}

- (void)selectNodeContents:(WKDOMNode *)node
{
    if (!node)
        return;
    _impl->selectNodeContents(*WebKit::toWebCoreNode(node));
}

- (WKDOMNode *)startContainer
{
    return WebKit::toWKDOMNode(&_impl->startContainer());
}

- (NSInteger)startOffset
{
    return _impl->startOffset();
}

- (WKDOMNode *)endContainer
{
    return WebKit::toWKDOMNode(&_impl->endContainer());
}

- (NSInteger)endOffset
{
    return _impl->endOffset();
}

- (NSString *)text
{
    return _impl->text();
}

- (BOOL)isCollapsed
{
    return _impl->collapsed();
}

- (NSArray *)textRects
{
    _impl->ownerDocument().updateLayoutIgnorePendingStylesheets();
    return createNSArray(WebCore::RenderObject::absoluteTextRects(makeSimpleRange(*_impl))).autorelease();
}

- (WKDOMRange *)rangeByExpandingToWordBoundaryByCharacters:(NSUInteger)characters inDirection:(WKDOMRangeDirection)direction
{
    auto range = makeSimpleRange(*_impl);
    auto newRange = rangeExpandedByCharactersInDirectionAtWordBoundary(makeDeprecatedLegacyPosition(direction == WKDOMRangeDirectionForward ? range.end : range.start), characters, direction == WKDOMRangeDirectionForward ? WebCore::SelectionDirection::Forward : WebCore::SelectionDirection::Backward);
    return [[[WKDOMRange alloc] _initWithImpl:createLiveRange(newRange).get()] autorelease];
}

@end

@implementation WKDOMRange (WKPrivate)

- (WKBundleRangeHandleRef)_copyBundleRangeHandleRef
{
    auto rangeHandle = WebKit::InjectedBundleRangeHandle::getOrCreate(_impl.get());
    return toAPI(rangeHandle.leakRef());
}

@end
