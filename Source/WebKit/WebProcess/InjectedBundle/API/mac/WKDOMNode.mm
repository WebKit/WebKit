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

static Ref<WebCore::Node> protectedImpl(WKDOMNode *node)
{
    return *node->_impl;
}

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

    protectedImpl(self)->insertBefore(*WebKit::toProtectedWebCoreNode(node).get(), WebKit::toProtectedWebCoreNode(refNode).get());
}

- (void)appendChild:(WKDOMNode *)node
{
    if (!node)
        return;

    protectedImpl(self)->appendChild(*WebKit::toProtectedWebCoreNode(node).get());
}

- (void)removeChild:(WKDOMNode *)node
{
    if (!node)
        return;

    protectedImpl(self)->removeChild(*WebKit::toProtectedWebCoreNode(node).get());
}

- (WKDOMDocument *)document
{
    return WebKit::toWKDOMDocument(protect(protectedImpl(self)->document()).ptr());
}

- (WKDOMNode *)parentNode
{
    return WebKit::toWKDOMNode(protect(protectedImpl(self)->parentNode()).get());
}

- (WKDOMNode *)firstChild
{
    return WebKit::toWKDOMNode(protect(protectedImpl(self)->firstChild()).get());
}

- (WKDOMNode *)lastChild
{
    return WebKit::toWKDOMNode(protect(protectedImpl(self)->lastChild()).get());
}

- (WKDOMNode *)previousSibling
{
    return WebKit::toWKDOMNode(protect(protectedImpl(self)->previousSibling()).get());
}

- (WKDOMNode *)nextSibling
{
    return WebKit::toWKDOMNode(protect(protectedImpl(self)->nextSibling()).get());
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
    return toAPILeakingRef(WebKit::InjectedBundleNodeHandle::getOrCreate(protectedImpl(self).ptr()));
}

@end
