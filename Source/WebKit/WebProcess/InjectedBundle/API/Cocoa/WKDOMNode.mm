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
#import "WKDOMNodePrivate.h"

#import "InjectedBundleNodeHandle.h"
#import "WKBundleAPICast.h"
#import "WKDOMInternals.h"
#import <WebCore/Document.h>
#import <WebCore/NodeInlines.h>
#import <WebCore/RenderObject.h>
#import <WebCore/SimpleRange.h>
#import <wtf/MainThread.h>
#import <wtf/cocoa/VectorCocoa.h>

@implementation WKDOMNode

- (id)_initWithImpl:(WebCore::Node*)impl
{
    self = [super init];
    if (!self)
        return nil;

    RELEASE_ASSERT(impl);
    _impl = impl;
    WebKit::WKDOMNodeCache().add(impl, self);

    return self;
}

- (void)dealloc
{
    ensureOnMainRunLoop([node = std::exchange(_impl, nullptr)] {
        WebKit::WKDOMNodeCache().remove(node.get());
    });
    [super dealloc];
}

- (void)insertNode:(WKDOMNode *)node before:(WKDOMNode *)refNode
{
    if (!node)
        return;

    protect(*_impl)->insertBefore(*protect(WebKit::toWebCoreNode(node)), protect(WebKit::toWebCoreNode(refNode)));
}

- (void)appendChild:(WKDOMNode *)node
{
    if (!node)
        return;

    protect(*_impl)->appendChild(*protect(WebKit::toWebCoreNode(node)));
}

- (void)removeChild:(WKDOMNode *)node
{
    if (!node)
        return;

    protect(*_impl)->removeChild(*protect(WebKit::toWebCoreNode(node)));
}

- (WKDOMDocument *)document
{
    return WebKit::toWKDOMDocument(protect(protect(*_impl)->document()).ptr());
}

- (WKDOMNode *)parentNode
{
    return WebKit::toWKDOMNode(protect(protect(*_impl)->parentNode()).get());
}

- (WKDOMNode *)firstChild
{
    return WebKit::toWKDOMNode(protect(protect(*_impl)->firstChild()).get());
}

- (WKDOMNode *)lastChild
{
    return WebKit::toWKDOMNode(protect(protect(*_impl)->lastChild()).get());
}

- (WKDOMNode *)previousSibling
{
    return WebKit::toWKDOMNode(protect(protect(*_impl)->previousSibling()).get());
}

- (WKDOMNode *)nextSibling
{
    return WebKit::toWKDOMNode(protect(protect(*_impl)->nextSibling()).get());
}

- (NSArray *)textRects
{
    Ref impl = *_impl;
    protect(impl->document())->updateLayout(WebCore::LayoutOptions::IgnorePendingStylesheets);
    if (!impl->renderer())
        return nil;
    return createNSArray(WebCore::RenderObject::absoluteTextRects(WebCore::makeRangeSelectingNodeContents(impl))).autorelease();
}

@end

@implementation WKDOMNode (WKPrivate)

- (WKBundleNodeHandleRef)_copyBundleNodeHandleRef
{
    return toAPILeakingRef(WebKit::InjectedBundleNodeHandle::getOrCreate(protect(*_impl).ptr()));
}

@end
