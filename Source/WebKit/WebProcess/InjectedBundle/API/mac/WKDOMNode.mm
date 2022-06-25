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

    _impl = impl;
    WebKit::WKDOMNodeCache().add(impl, self);

    return self;
}

- (void)dealloc
{
    ensureOnMainRunLoop([node = WTFMove(_impl)] {
        WebKit::WKDOMNodeCache().remove(node.get());
    });
    [super dealloc];
}

- (void)insertNode:(WKDOMNode *)node before:(WKDOMNode *)refNode
{
    if (!node)
        return;

    _impl->insertBefore(*WebKit::toWebCoreNode(node), WebKit::toWebCoreNode(refNode));
}

- (void)appendChild:(WKDOMNode *)node
{
    if (!node)
        return;

    _impl->appendChild(*WebKit::toWebCoreNode(node));
}

- (void)removeChild:(WKDOMNode *)node
{
    if (!node)
        return;

    _impl->removeChild(*WebKit::toWebCoreNode(node));
}

- (WKDOMDocument *)document
{
    return WebKit::toWKDOMDocument(&_impl->document());
}

- (WKDOMNode *)parentNode
{
    return WebKit::toWKDOMNode(_impl->parentNode());
}

- (WKDOMNode *)firstChild
{
    return WebKit::toWKDOMNode(_impl->firstChild());
}

- (WKDOMNode *)lastChild
{
    return WebKit::toWKDOMNode(_impl->lastChild());
}

- (WKDOMNode *)previousSibling
{
    return WebKit::toWKDOMNode(_impl->previousSibling());
}

- (WKDOMNode *)nextSibling
{
    return WebKit::toWKDOMNode(_impl->nextSibling());
}

- (NSArray *)textRects
{
    _impl->document().updateLayoutIgnorePendingStylesheets();
    if (!_impl->renderer())
        return nil;
    return createNSArray(WebCore::RenderObject::absoluteTextRects(WebCore::makeRangeSelectingNodeContents(*_impl))).autorelease();
}

@end

@implementation WKDOMNode (WKPrivate)

- (WKBundleNodeHandleRef)_copyBundleNodeHandleRef
{
    return toAPI(WebKit::InjectedBundleNodeHandle::getOrCreate(_impl.get()).leakRef());
}

@end
