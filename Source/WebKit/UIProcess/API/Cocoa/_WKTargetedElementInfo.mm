/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "_WKTargetedElementInfo.h"

#import "APIFrameTreeNode.h"
#import "WKObject.h"
#import "WebPageProxy.h"
#import "_WKFrameTreeNodeInternal.h"
#import "_WKTargetedElementInfoInternal.h"
#import <WebCore/ShareableBitmap.h>
#import <WebCore/WebCoreObjCExtras.h>
#import <wtf/BlockPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

@implementation _WKTargetedElementInfo

- (void)dealloc
{
    if (WebCoreObjCScheduleDeallocateOnMainRunLoop(_WKTargetedElementInfo.class, self))
        return;
    _info->API::TargetedElementInfo::~TargetedElementInfo();
    [super dealloc];
}

- (API::Object&)_apiObject
{
    return *_info;
}

- (_WKTargetedElementPosition)positionType
{
    switch (_info->positionType()) {
    case WebCore::PositionType::Static:
        return _WKTargetedElementPositionStatic;
    case WebCore::PositionType::Relative:
        return _WKTargetedElementPositionRelative;
    case WebCore::PositionType::Absolute:
        return _WKTargetedElementPositionAbsolute;
    case WebCore::PositionType::Sticky:
        return _WKTargetedElementPositionSticky;
    case WebCore::PositionType::Fixed:
        return _WKTargetedElementPositionFixed;
    }
}

- (CGRect)bounds
{
    return _info->boundsInRootView();
}

- (CGRect)boundsInWebView
{
    return _info->boundsInWebView();
}

- (CGRect)boundsInClientCoordinates
{
    return _info->boundsInClientCoordinates();
}

- (NSArray<NSString *> *)selectors
{
    if (_info->selectors().isEmpty())
        return @[ ];

    if (_info->isInShadowTree())
        return @[ ];

    return createNSArray(_info->selectors().first()).autorelease();
}

- (NSArray<NSArray<NSString *> *> *)selectorsIncludingShadowHosts
{
    RetainPtr result = adoptNS([[NSMutableArray alloc] initWithCapacity:_info->selectors().size()]);
    for (auto& selectors : _info->selectors()) {
        RetainPtr nsSelectors = adoptNS([[NSMutableArray alloc] initWithCapacity:selectors.size()]);
        for (auto& selector : selectors)
            [nsSelectors addObject:selector.createNSString().get()];
        [result addObject:nsSelectors.get()];
    }
    return result.autorelease();
}

- (NSString *)renderedText
{
    return _info->renderedText().createNSString().autorelease();
}

- (NSString *)searchableText
{
    return _info->searchableText().createNSString().autorelease();
}

- (NSString *)screenReaderText
{
    return _info->screenReaderText().createNSString().autorelease();
}

- (_WKRectEdge)offsetEdges
{
    _WKRectEdge edges = _WKRectEdgeNone;
    auto coreEdges = _info->offsetEdges();
    if (coreEdges.top())
        edges |= _WKRectEdgeTop;
    if (coreEdges.left())
        edges |= _WKRectEdgeLeft;
    if (coreEdges.bottom())
        edges |= _WKRectEdgeBottom;
    if (coreEdges.right())
        edges |= _WKRectEdgeRight;
    return edges;
}

- (void)getChildFrames:(void(^)(NSArray<_WKFrameTreeNode *> *))completion
{
    return _info->childFrames([completion = makeBlockPtr(completion)](auto&& nodes) {
        completion(createNSArray(WTFMove(nodes), [](API::FrameTreeNode& node) {
            return wrapper(node);
        }).autorelease());
    });
}

- (BOOL)isSameElement:(_WKTargetedElementInfo *)other
{
    return _info->isSameElement(*other->_info);
}

- (BOOL)isNearbyTarget
{
    return _info->isNearbyTarget();
}

- (BOOL)isInVisibilityAdjustmentSubtree
{
    return _info->isInVisibilityAdjustmentSubtree();
}

- (BOOL)hasLargeReplacedDescendant
{
    return _info->hasLargeReplacedDescendant();
}

- (NSSet<NSURL *> *)mediaAndLinkURLs
{
    RetainPtr result = adoptNS([NSMutableSet<NSURL *> new]);
    for (auto& url : _info->mediaAndLinkURLs()) {
        if (RetainPtr nsURL = url.createNSURL())
            [result addObject:nsURL.get()];
    }
    return result.autorelease();
}

- (BOOL)isPseudoElement
{
    return _info->isPseudoElement();
}

- (BOOL)isInShadowTree
{
    return _info->isInShadowTree();
}

- (BOOL)hasAudibleMedia
{
    return _info->hasAudibleMedia();
}

- (void)takeSnapshotWithCompletionHandler:(void(^)(CGImageRef))completion
{
    return _info->takeSnapshot([completion = makeBlockPtr(completion)](std::optional<WebCore::ShareableBitmapHandle>&& imageHandle) mutable {
        if (!imageHandle)
            return completion(nullptr);

        if (RefPtr bitmap = WebCore::ShareableBitmap::create(WTFMove(*imageHandle), WebCore::SharedMemory::Protection::ReadOnly))
            return completion(bitmap->createPlatformImage(WebCore::DontCopyBackingStore).get());

        completion(nullptr);
    });
}

- (NSString *)debugDescription
{
    auto firstSelector = [&]() -> String {
        auto& allSelectors = _info->selectors();
        if (allSelectors.isEmpty())
            return { };

        if (allSelectors.last().isEmpty())
            return { };

        // Most relevant selector for the final target element (after all enclosing shadow
        // roots have been resolved).
        return allSelectors.last().first();
    }();

    auto bounds = _info->boundsInRootView();
    return [NSString stringWithFormat:@"<%@ %p \"%@\" at {{%.0f,%.0f},{%.0f,%.0f}}>"
        , self.class, self, firstSelector.createNSString().get()
        , bounds.x(), bounds.y(), bounds.width(), bounds.height()];
}

@end
