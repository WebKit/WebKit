/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)

#import "UIKitSPI.h"
#import <WebCore/DragActions.h>
#import <WebCore/DragData.h>
#import <WebCore/Path.h>
#import <WebCore/TextIndicator.h>
#import <WebCore/WebItemProviderPasteboard.h>
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/URL.h>
#import <wtf/Vector.h>

namespace WebCore {
struct DragItem;
struct NodeIdentifierType;
using NodeIdentifier = ObjectIdentifier<NodeIdentifierType>;
}

namespace WebKit {

struct DragSourceState {
    OptionSet<WebCore::DragSourceAction> action;
    CGRect dragPreviewFrameInRootViewCoordinates { CGRectZero };
    using DragPreviewContentType = Variant<RetainPtr<UIImage>, RetainPtr<UIView>>;
    DragPreviewContentType dragPreviewContent;
    RefPtr<WebCore::TextIndicator> textIndicator;
    std::optional<WebCore::Path> visiblePath;
    String linkTitle;
    URL linkURL;
    bool possiblyNeedsDragPreviewUpdate { true };
    bool containsSelection { false };

    NSInteger itemIdentifier { 0 };
};

using DragItemToPreviewMap = HashMap<RetainPtr<UIDragItem>, RetainPtr<UITargetedDragPreview>>;

enum class AddPreviewViewToContainer : bool;

class DragDropInteractionState {
public:
    bool anyActiveDragSourceContainsSelection() const;

    // These helper methods are unique to UIDragInteraction.
    void prepareForDragSession(id <UIDragSession>, dispatch_block_t completionHandler);
    void dragSessionWillBegin();
    void stageDragItem(const WebCore::DragItem&, DragSourceState::DragPreviewContentType);
    bool hasStagedDragSource() const;
    const DragSourceState& stagedDragSource() const { return m_stagedDragSource.value(); }
    enum class DidBecomeActive : bool { No, Yes };
    void clearStagedDragSource(DidBecomeActive = DidBecomeActive::No);
    UITargetedDragPreview *previewForLifting(UIDragItem *, UIView *contentView, UIView *previewContainer, RefPtr<WebCore::TextIndicator>&&) const;
    UITargetedDragPreview *previewForCancelling(UIDragItem *, UIView *contentView, UIView *previewContainer);
    void dragSessionWillDelaySetDownAnimation(dispatch_block_t completion);
    bool shouldRequestAdditionalItemForDragSession(id <UIDragSession>) const;
    void dragSessionWillRequestAdditionalItem(void (^completion)(NSArray <UIDragItem *> *));

    void dropSessionDidEnterOrUpdate(id <UIDropSession>, const WebCore::DragData&);
    void dropSessionDidExit() { m_dropSession = nil; }
    void dropSessionWillPerformDrop() { m_isPerformingDrop = true; }
    void setElementIdentifier(const std::optional<WebCore::NodeIdentifier>&);

    void dragAndDropSessionsDidBecomeInactive();

    CGPoint adjustedPositionForDragEnd() const { return m_adjustedPositionForDragEnd; }
    bool didBeginDragging() const { return m_didBeginDragging; }
    bool isPerformingDrop() const { return m_isPerformingDrop; }
    id<UIDragSession> dragSession() const { return m_dragSession.get(); }
    id<UIDropSession> dropSession() const { return m_dropSession.get(); }
    BlockPtr<void()> takeDragStartCompletionBlock() { return WTFMove(m_dragStartCompletionBlock); }
    BlockPtr<void(NSArray<UIDragItem *> *)> takeAddDragItemCompletionBlock() { return WTFMove(m_addDragItemCompletionBlock); }
    Vector<RetainPtr<UIView>> takePreviewViewsForDragCancel() { return std::exchange(m_previewViewsForDragCancel, { }); }
    std::optional<WebCore::NodeIdentifier> nodeIdentifier() const { return m_nodeIdentifier; }

    void addDefaultDropPreview(UIDragItem *, UITargetedDragPreview *);
    UITargetedDragPreview *finalDropPreview(UIDragItem *) const;
    void deliverDelayedDropPreview(UIView *contentView, UIView *previewContainer, RefPtr<WebCore::TextIndicator>&&);
    void deliverDelayedDropPreview(UIView *contentView, CGRect unobscuredContentRect, NSArray<UIDragItem *> *, const Vector<WebCore::IntRect>& placeholderRects);

private:
    void updatePreviewsForActiveDragSources();
    std::optional<DragSourceState> activeDragSourceForItem(UIDragItem *) const;
    UITargetedDragPreview *defaultDropPreview(UIDragItem *) const;

    RetainPtr<UITargetedDragPreview> createDragPreviewInternal(UIDragItem *, UIView *contentView, UIView *previewContainer, AddPreviewViewToContainer, const RefPtr<WebCore::TextIndicator>&&) const;

    CGPoint m_lastGlobalPosition { CGPointZero };
    CGPoint m_adjustedPositionForDragEnd { CGPointZero };
    bool m_didBeginDragging { false };
    bool m_isPerformingDrop { false };
    RetainPtr<id <UIDragSession>> m_dragSession;
    RetainPtr<id <UIDropSession>> m_dropSession;
    BlockPtr<void()> m_dragStartCompletionBlock;
    BlockPtr<void(NSArray<UIDragItem *> *)> m_addDragItemCompletionBlock;
    Vector<RetainPtr<UIView>> m_previewViewsForDragCancel;

    std::optional<DragSourceState> m_stagedDragSource;
    Vector<DragSourceState> m_activeDragSources;
    DragItemToPreviewMap m_defaultDropPreviews;
    DragItemToPreviewMap m_finalDropPreviews;
    std::optional<WebCore::NodeIdentifier> m_nodeIdentifier;
};

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
