/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#import "DragDropInteractionState.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)

#import "Logging.h"
#import <WebCore/ColorCocoa.h>
#import <WebCore/DragItem.h>
#import <WebCore/Image.h>
#import <WebCore/LocalCurrentTraitCollection.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {
using namespace WebCore;

enum class AddPreviewViewToContainer : bool { No, Yes };

static UIDragItem *dragItemMatchingIdentifier(id <UIDragSession> session, NSInteger identifier)
{
    for (UIDragItem *item in session.items) {
        id context = item.privateLocalContext;
        if ([context isKindOfClass:[NSNumber class]] && [context integerValue] == identifier)
            return item;
    }
    return nil;
}

static RetainPtr<UITargetedDragPreview> createTargetedDragPreview(UIImage *image, UIView *rootView, UIView *previewContainer, const FloatRect& frameInRootViewCoordinates, const Vector<FloatRect>& clippingRectsInFrameCoordinates, UIColor *backgroundColor, UIBezierPath *visiblePath, AddPreviewViewToContainer addPreviewViewToContainer)
{
    if (frameInRootViewCoordinates.isEmpty() || !image || !previewContainer.window)
        return nil;

    FloatRect frameInContainerCoordinates = [rootView convertRect:frameInRootViewCoordinates toView:previewContainer];
    if (frameInContainerCoordinates.isEmpty())
        return nil;

    FloatSize scalingRatio = frameInContainerCoordinates.size() / frameInRootViewCoordinates.size();
    auto clippingRectValuesInFrameCoordinates = createNSArray(clippingRectsInFrameCoordinates, [&] (auto rect) {
        rect.scale(scalingRatio);
        return [NSValue valueWithCGRect:rect];
    });

    auto imageView = adoptNS([[UIImageView alloc] initWithImage:image]);
    [imageView setFrame:frameInContainerCoordinates];

    RetainPtr<UIDragPreviewParameters> parameters;
    if ([clippingRectValuesInFrameCoordinates count])
        parameters = adoptNS([[UIDragPreviewParameters alloc] initWithTextLineRects:clippingRectValuesInFrameCoordinates.get()]);
    else
        parameters = adoptNS([[UIDragPreviewParameters alloc] init]);

    if (backgroundColor)
        [parameters setBackgroundColor:backgroundColor];

    if (visiblePath)
        [parameters setVisiblePath:visiblePath];

    if (addPreviewViewToContainer == AddPreviewViewToContainer::Yes)
        [previewContainer addSubview:imageView.get()];

    CGPoint centerInContainerCoordinates = { CGRectGetMidX(frameInContainerCoordinates), CGRectGetMidY(frameInContainerCoordinates) };
    auto target = adoptNS([[UIDragPreviewTarget alloc] initWithContainer:previewContainer center:centerInContainerCoordinates]);
    return adoptNS([[UITargetedDragPreview alloc] initWithView:imageView.get() parameters:parameters.get() target:target.get()]);
}

static RetainPtr<UIImage> uiImageForImage(Image* image)
{
    if (!image)
        return nullptr;

    auto nativeImage = image->nativeImage();
    if (!nativeImage)
        return nullptr;

    return adoptNS([[UIImage alloc] initWithCGImage:nativeImage->platformImage().get()]);
}

static bool shouldUseDragImageToCreatePreviewForDragSource(const DragSourceState& source)
{
    if (!source.image)
        return false;

#if ENABLE(INPUT_TYPE_COLOR)
    if (source.action.contains(DragSourceAction::Color))
        return true;
#endif

#if ENABLE(MODEL_ELEMENT)
    if (source.action.contains(DragSourceAction::Model))
        return true;
#endif

    return source.action.containsAny({ DragSourceAction::DHTML, DragSourceAction::Image });
}

static bool shouldUseVisiblePathToCreatePreviewForDragSource(const DragSourceState& source)
{
    if (!source.visiblePath)
        return false;

#if ENABLE(INPUT_TYPE_COLOR)
    if (source.action.contains(DragSourceAction::Color))
        return true;
#endif

    return false;
}

static bool shouldUseTextIndicatorToCreatePreviewForDragSource(const DragSourceState& source)
{
    if (!source.indicatorData)
        return false;

    if (source.action.containsAny({ DragSourceAction::Link, DragSourceAction::Selection }))
        return true;

#if ENABLE(ATTACHMENT_ELEMENT)
    if (source.action.contains(DragSourceAction::Attachment))
        return true;
#endif

    return false;
}

static bool canUpdatePreviewForActiveDragSource(const DragSourceState& source)
{
    if (!source.possiblyNeedsDragPreviewUpdate)
        return false;

#if ENABLE(INPUT_TYPE_COLOR)
    if (source.action.contains(DragSourceAction::Color))
        return true;
#endif

    if (source.action.contains(DragSourceAction::Link) && !source.action.contains(DragSourceAction::Image))
        return true;

    return false;
}

std::optional<DragSourceState> DragDropInteractionState::activeDragSourceForItem(UIDragItem *item) const
{
    if (![item.privateLocalContext isKindOfClass:[NSNumber class]])
        return std::nullopt;

    auto identifier = [(NSNumber *)item.privateLocalContext integerValue];
    for (auto& source : m_activeDragSources) {
        if (source.itemIdentifier == identifier)
            return source;
    }
    return std::nullopt;
}

bool DragDropInteractionState::anyActiveDragSourceContainsSelection() const
{
    for (auto& source : m_activeDragSources) {
        if (source.containsSelection)
            return true;
    }

    return false;
}

void DragDropInteractionState::prepareForDragSession(id <UIDragSession> session, dispatch_block_t completionHandler)
{
    m_dragSession = session;
    m_dragStartCompletionBlock = completionHandler;
}

void DragDropInteractionState::dragSessionWillBegin()
{
    m_didBeginDragging = true;
    updatePreviewsForActiveDragSources();
}

void DragDropInteractionState::addDefaultDropPreview(UIDragItem *item, UITargetedDragPreview *preview)
{
    m_defaultDropPreviews.add(item, preview);
}

UITargetedDragPreview *DragDropInteractionState::defaultDropPreview(UIDragItem *item) const
{
    return m_defaultDropPreviews.get(item).get();
}

UITargetedDragPreview *DragDropInteractionState::finalDropPreview(UIDragItem *item) const
{
    return m_finalDropPreviews.get(item).get();
}

inline static bool dragItemSupportsAsynchronousUpdates()
{
    static bool hasSupport = [UIDragItem instancesRespondToSelector:@selector(_setNeedsDropPreviewUpdate)];
    return hasSupport;
}

void DragDropInteractionState::deliverDelayedDropPreview(UIView *contentView, UIView *previewContainer, const WebCore::TextIndicatorData& indicator)
{
    auto textIndicatorImage = uiImageForImage(indicator.contentImage.get());
    auto preview = createTargetedDragPreview(textIndicatorImage.get(), contentView, previewContainer, indicator.textBoundingRectInRootViewCoordinates, indicator.textRectsInBoundingRectCoordinates, cocoaColor(indicator.estimatedBackgroundColor).get(), nil, AddPreviewViewToContainer::No);
    if (!preview)
        return;

    for (auto item : m_defaultDropPreviews.keys()) {
        m_finalDropPreviews.add(item, preview.get());
        if (dragItemSupportsAsynchronousUpdates())
            [item _setNeedsDropPreviewUpdate];
    }
}

void DragDropInteractionState::deliverDelayedDropPreview(UIView *contentView, CGRect unobscuredContentRect, NSArray<UIDragItem *> *items, const Vector<IntRect>& placeholderRects)
{
    if (items.count != placeholderRects.size()) {
        RELEASE_LOG_ERROR(DragAndDrop, "Failed to animate image placeholders: number of drag items (%tu) does not match number of placeholders (%tu)", items.count, placeholderRects.size());
        return;
    }

    for (size_t i = 0; i < placeholderRects.size(); ++i) {
        UIDragItem *item = [items objectAtIndex:i];
        auto& placeholderRect = placeholderRects[i];
        auto defaultPreview = defaultDropPreview(item);
        auto defaultPreviewSize = [defaultPreview size];
        if (!defaultPreview || defaultPreviewSize.width <= 0 || defaultPreviewSize.height <= 0 || placeholderRect.isEmpty())
            continue;

        FloatRect previewIntersectionRect = enclosingIntRect(CGRectIntersection(unobscuredContentRect, placeholderRect));
        if (previewIntersectionRect.isEmpty()) {
            // If the preview rect is completely offscreen, don't bother trying to clip out or scale the default preview;
            // simply retarget the default preview.
            auto target = adoptNS([[UIDragPreviewTarget alloc] initWithContainer:contentView center:placeholderRect.center()]);
            m_finalDropPreviews.add(item, [defaultPreview retargetedPreviewWithTarget:target.get()]);
            if (dragItemSupportsAsynchronousUpdates())
                [item _setNeedsDropPreviewUpdate];
            continue;
        }

        // Targeted previews don't clip to the bounds of any enclosing view; this means that when targeting previews outside
        // the visible bounds of the content view, the preview will spill out the web view. This is most noticeable when
        // dropping a tall image into Mail compose on iOS 13, where the bottom of the compose window is not flush against
        // the bottom of the window. To mitigate this, we use the preview target's `visiblePath` property to clip the default
        // drop preview's view, by the same proportion that the final placeholder image is clipped (with respect to the
        // unobscured content rect).
        auto previewBounds = [defaultPreview view].bounds;
        auto insetPreviewBounds = UIEdgeInsetsInsetRect(previewBounds, {
            (previewIntersectionRect.y() - placeholderRect.y()) / placeholderRect.height() * previewBounds.size.height,
            (previewIntersectionRect.x() - placeholderRect.x()) / placeholderRect.width() * previewBounds.size.width,
            (placeholderRect.maxY() - previewIntersectionRect.maxY()) / placeholderRect.height() * previewBounds.size.height,
            (placeholderRect.maxX() - previewIntersectionRect.maxX()) / placeholderRect.width() * previewBounds.size.width
        });

        auto transform = CGAffineTransformMakeScale(placeholderRect.width() / defaultPreviewSize.width, placeholderRect.height() / defaultPreviewSize.height);
        auto target = adoptNS([[UIDragPreviewTarget alloc] initWithContainer:contentView center:previewIntersectionRect.center() transform:transform]);
        [defaultPreview parameters].visiblePath = [UIBezierPath bezierPathWithRect:insetPreviewBounds];
        m_finalDropPreviews.add(item, adoptNS([[UITargetedDragPreview alloc] initWithView:[defaultPreview view] parameters:[defaultPreview parameters] target:target.get()]));
        if (dragItemSupportsAsynchronousUpdates())
            [item _setNeedsDropPreviewUpdate];
    }
}

UITargetedDragPreview *DragDropInteractionState::previewForLifting(UIDragItem *item, UIView *contentView, UIView *previewContainer, const std::optional<WebCore::TextIndicatorData>& indicator) const
{
    return createDragPreviewInternal(item, contentView, previewContainer, AddPreviewViewToContainer::No, indicator).autorelease();
}

UITargetedDragPreview *DragDropInteractionState::previewForCancelling(UIDragItem *item, UIView *contentView, UIView *previewContainer)
{
    auto preview = createDragPreviewInternal(item, contentView, previewContainer, AddPreviewViewToContainer::Yes, std::nullopt);
    m_previewViewForDragCancel = [preview view];
    return preview.autorelease();
}

RetainPtr<UITargetedDragPreview> DragDropInteractionState::createDragPreviewInternal(UIDragItem *item, UIView *contentView, UIView *previewContainer, AddPreviewViewToContainer addPreviewViewToContainer, const std::optional<WebCore::TextIndicatorData>& indicator) const
{
    auto foundSource = activeDragSourceForItem(item);
    if (!foundSource)
        return nil;

    if (indicator) {
        // If the context menu preview was created using the snapshot mechanism,
        // the drag preview should be created likewise, so that the size and position
        // of both previews match.
        auto textIndicatorImage = uiImageForImage(indicator->contentImage.get());
        return createTargetedDragPreview(textIndicatorImage.get(), contentView, previewContainer, indicator->textBoundingRectInRootViewCoordinates, indicator->textRectsInBoundingRectCoordinates, cocoaColor(indicator->estimatedBackgroundColor).get(), nil, addPreviewViewToContainer).autorelease();
    }

    auto& source = foundSource.value();
    if (shouldUseDragImageToCreatePreviewForDragSource(source)) {
        if (shouldUseVisiblePathToCreatePreviewForDragSource(source)) {
            auto path = source.visiblePath.value();
            UIBezierPath *visiblePath = [UIBezierPath bezierPathWithCGPath:path.platformPath()];
            return createTargetedDragPreview(source.image.get(), contentView, previewContainer, source.dragPreviewFrameInRootViewCoordinates, { }, nil, visiblePath, addPreviewViewToContainer).autorelease();
        }
        return createTargetedDragPreview(source.image.get(), contentView, previewContainer, source.dragPreviewFrameInRootViewCoordinates, { }, nil, nil, addPreviewViewToContainer).autorelease();
    }

    if (shouldUseTextIndicatorToCreatePreviewForDragSource(source)) {
        auto indicator = source.indicatorData.value();
        auto textIndicatorImage = uiImageForImage(indicator.contentImage.get());
        return createTargetedDragPreview(textIndicatorImage.get(), contentView, previewContainer, indicator.textBoundingRectInRootViewCoordinates, indicator.textRectsInBoundingRectCoordinates, cocoaColor(indicator.estimatedBackgroundColor).get(), nil, addPreviewViewToContainer).autorelease();
    }

    return nil;
}

bool DragDropInteractionState::shouldRequestAdditionalItemForDragSession(id <UIDragSession> session) const
{
    return m_dragSession == session && !m_addDragItemCompletionBlock && !m_dragStartCompletionBlock;
}

void DragDropInteractionState::dragSessionWillRequestAdditionalItem(void (^completion)(NSArray <UIDragItem *> *))
{
    clearStagedDragSource();
    m_addDragItemCompletionBlock = completion;
}

void DragDropInteractionState::dropSessionDidEnterOrUpdate(id <UIDropSession> session, const DragData& dragData)
{
    m_dropSession = session;
    m_lastGlobalPosition = dragData.globalPosition();
}

void DragDropInteractionState::stageDragItem(const DragItem& item, UIImage *dragImage)
{
    static NSInteger currentDragSourceItemIdentifier = 0;

    m_adjustedPositionForDragEnd = item.eventPositionInContentCoordinates;
    m_stagedDragSource = {{
        item.sourceAction,
        item.dragPreviewFrameInRootViewCoordinates,
        dragImage,
        item.image.indicatorData(),
        item.image.visiblePath(),
        item.title.isEmpty() ? nil : (NSString *)item.title,
        item.url.isEmpty() ? nil : (NSURL *)item.url,
        true, // We assume here that drag previews need to be updated until proven otherwise in updatePreviewsForActiveDragSources().
        item.containsSelection,
        ++currentDragSourceItemIdentifier
    }};
}

bool DragDropInteractionState::hasStagedDragSource() const
{
    return m_stagedDragSource && !stagedDragSource().action.isEmpty();
}

void DragDropInteractionState::clearStagedDragSource(DidBecomeActive didBecomeActive)
{
    if (didBecomeActive == DidBecomeActive::Yes)
        m_activeDragSources.append(stagedDragSource());
    m_stagedDragSource = std::nullopt;
}

void DragDropInteractionState::dragAndDropSessionsDidBecomeInactive()
{
    if (auto previewView = takePreviewViewForDragCancel())
        [previewView removeFromSuperview];

    // If any of UIKit's completion blocks are still in-flight when the drag interaction ends, we need to ensure that they are still invoked
    // to prevent UIKit from getting into an inconsistent state.
    if (auto completionBlock = takeAddDragItemCompletionBlock())
        completionBlock(@[ ]);

    if (auto completionBlock = takeDragStartCompletionBlock())
        completionBlock();
}

void DragDropInteractionState::updatePreviewsForActiveDragSources()
{
    for (auto& source : m_activeDragSources) {
        if (!canUpdatePreviewForActiveDragSource(source))
            continue;

        UIDragItem *dragItem = dragItemMatchingIdentifier(m_dragSession.get(), source.itemIdentifier);
        if (!dragItem)
            continue;

        if (source.action.contains(DragSourceAction::Link)) {
            dragItem.previewProvider = [title = retainPtr((NSString *)source.linkTitle), url = retainPtr((NSURL *)source.linkURL)] () -> UIDragPreview * {
                RetainPtr preview = [UIDragPreview previewForURL:url.get() title:title.get()];
#if PLATFORM(VISION)
                // FIXME: This is a slightly unfortunate since we end up copying the preview parameters,
                // and also create an extra `UIDragPreview` on visionOS. We can remove this workaround
                // once UIKit addresses <rdar://114204432>.
                auto adjustedParameters = [preview parameters];
                adjustedParameters.backgroundColor = [UIColor colorWithDynamicProvider:[] (UITraitCollection *traitCollection) -> UIColor * {
                    WebCore::LocalCurrentTraitCollection localCurrentTraitCollection(traitCollection);
                    return [UIColor.systemBackgroundColor resolvedColorWithTraitCollection:UITraitCollection.currentTraitCollection];
                }];
                preview = adoptNS([[UIDragPreview alloc] initWithView:[preview view] parameters:adjustedParameters]);
#endif // PLATFORM(VISION)
                return preview.autorelease();
            };
        }
#if ENABLE(INPUT_TYPE_COLOR)
        else if (source.action.contains(DragSourceAction::Color)) {
            dragItem.previewProvider = [image = source.image] () -> UIDragPreview * {
                auto imageView = adoptNS([[UIImageView alloc] initWithImage:image.get()]);
                auto parameters = adoptNS([[UIDragPreviewParameters alloc] initWithTextLineRects:@[ [NSValue valueWithCGRect:[imageView bounds]] ]]);
                return adoptNS([[UIDragPreview alloc] initWithView:imageView.get() parameters:parameters.get()]).autorelease();
            };
        }
#endif

        source.possiblyNeedsDragPreviewUpdate = false;
    }
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
