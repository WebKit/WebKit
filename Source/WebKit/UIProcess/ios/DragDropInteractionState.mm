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

#import "config.h"
#import "DragDropInteractionState.h"

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)

#import "Logging.h"
#import <WebCore/DragItem.h>
#import <WebCore/Image.h>
#import <wtf/cocoa/VectorCocoa.h>

namespace WebKit {
using namespace WebCore;

static UIDragItem *dragItemMatchingIdentifier(id <UIDragSession> session, NSInteger identifier)
{
    for (UIDragItem *item in session.items) {
        id context = item.privateLocalContext;
        if ([context isKindOfClass:[NSNumber class]] && [context integerValue] == identifier)
            return item;
    }
    return nil;
}

static RetainPtr<UITargetedDragPreview> createTargetedDragPreview(UIImage *image, UIView *rootView, UIView *previewContainer, const FloatRect& frameInRootViewCoordinates, const Vector<FloatRect>& clippingRectsInFrameCoordinates, UIColor *backgroundColor, UIBezierPath *visiblePath)
{
    if (frameInRootViewCoordinates.isEmpty() || !image || !previewContainer.window)
        return nullptr;

    FloatRect frameInContainerCoordinates = [rootView convertRect:frameInRootViewCoordinates toView:previewContainer];
    if (frameInContainerCoordinates.isEmpty())
        return nullptr;

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

    CGPoint centerInContainerCoordinates = { CGRectGetMidX(frameInContainerCoordinates), CGRectGetMidY(frameInContainerCoordinates) };
    auto target = adoptNS([[UIDragPreviewTarget alloc] initWithContainer:previewContainer center:centerInContainerCoordinates]);
    return adoptNS([[UITargetedDragPreview alloc] initWithView:imageView.get() parameters:parameters.get() target:target.get()]);
}

static RetainPtr<UIImage> uiImageForImage(Image* image)
{
    if (!image)
        return nullptr;

    auto cgImage = image->nativeImage();
    if (!cgImage)
        return nullptr;

    return adoptNS([[UIImage alloc] initWithCGImage:cgImage.get()]);
}

static bool shouldUseDragImageToCreatePreviewForDragSource(const DragSourceState& source)
{
    if (!source.image)
        return false;

#if ENABLE(INPUT_TYPE_COLOR)
    if (source.action.contains(DragSourceAction::Color))
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

Optional<DragSourceState> DragDropInteractionState::activeDragSourceForItem(UIDragItem *item) const
{
    if (![item.privateLocalContext isKindOfClass:[NSNumber class]])
        return WTF::nullopt;

    auto identifier = [(NSNumber *)item.privateLocalContext integerValue];
    for (auto& source : m_activeDragSources) {
        if (source.itemIdentifier == identifier)
            return source;
    }
    return WTF::nullopt;
}

bool DragDropInteractionState::anyActiveDragSourceIs(WebCore::DragSourceAction action) const
{
    for (auto& source : m_activeDragSources) {
        if (source.action.contains(action))
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

void DragDropInteractionState::setDefaultDropPreview(UIDragItem *item, UITargetedDragPreview *preview)
{
    m_defaultDropPreviews.append({ item, preview });
}

UITargetedDragPreview *DragDropInteractionState::defaultDropPreview(UIDragItem *item) const
{
    auto matchIndex = m_defaultDropPreviews.findMatching([&] (auto& itemAndPreview) {
        return itemAndPreview.item == item;
    });
    return matchIndex == notFound ? nil : m_defaultDropPreviews[matchIndex].preview.get();
}

BlockPtr<void(UITargetedDragPreview *)> DragDropInteractionState::dropPreviewProvider(UIDragItem *item)
{
    auto matchIndex = m_delayedItemPreviewProviders.findMatching([&] (auto& itemAndProvider) {
        return itemAndProvider.item == item;
    });

    if (matchIndex == notFound)
        return nil;

    return m_delayedItemPreviewProviders[matchIndex].provider;
}

void DragDropInteractionState::prepareForDelayedDropPreview(UIDragItem *item, void(^provider)(UITargetedDragPreview *preview))
{
    m_delayedItemPreviewProviders.append({ item, provider });
}

void DragDropInteractionState::deliverDelayedDropPreview(UIView *contentView, UIView *previewContainer, const WebCore::TextIndicatorData& indicator)
{
    if (m_delayedItemPreviewProviders.isEmpty())
        return;

    auto textIndicatorImage = uiImageForImage(indicator.contentImage.get());
    auto preview = createTargetedDragPreview(textIndicatorImage.get(), contentView, previewContainer, indicator.textBoundingRectInRootViewCoordinates, indicator.textRectsInBoundingRectCoordinates, [UIColor colorWithCGColor:cachedCGColor(indicator.estimatedBackgroundColor)], nil);
    for (auto& itemAndPreviewProvider : m_delayedItemPreviewProviders)
        itemAndPreviewProvider.provider(preview.get());
    m_delayedItemPreviewProviders.clear();
}

void DragDropInteractionState::deliverDelayedDropPreview(UIView *contentView, CGRect unobscuredContentRect, NSArray<UIDragItem *> *items, const Vector<IntRect>& placeholderRects)
{
    if (items.count != placeholderRects.size()) {
        RELEASE_LOG(DragAndDrop, "Failed to animate image placeholders: number of drag items (%tu) does not match number of placeholders (%tu)", items.count, placeholderRects.size());
        clearAllDelayedItemPreviewProviders();
        return;
    }

    for (size_t i = 0; i < placeholderRects.size(); ++i) {
        UIDragItem *item = [items objectAtIndex:i];
        auto& placeholderRect = placeholderRects[i];
        auto provider = dropPreviewProvider(item);
        if (!provider)
            continue;

        auto defaultPreview = defaultDropPreview(item);
        auto defaultPreviewSize = [defaultPreview size];
        if (!defaultPreview || defaultPreviewSize.width <= 0 || defaultPreviewSize.height <= 0 || placeholderRect.isEmpty()) {
            provider(nil);
            continue;
        }

        FloatRect previewIntersectionRect = enclosingIntRect(CGRectIntersection(unobscuredContentRect, placeholderRect));
        if (previewIntersectionRect.isEmpty()) {
            // If the preview rect is completely offscreen, don't bother trying to clip out or scale the default preview;
            // simply retarget the default preview.
            auto target = adoptNS([[UIDragPreviewTarget alloc] initWithContainer:contentView center:placeholderRect.center()]);
            provider([defaultPreview retargetedPreviewWithTarget:target.get()]);
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
        auto newPreview = adoptNS([[UITargetedDragPreview alloc] initWithView:[defaultPreview view] parameters:[defaultPreview parameters] target:target.get()]);
        provider(newPreview.get());
    }

    m_delayedItemPreviewProviders.clear();
}

void DragDropInteractionState::clearAllDelayedItemPreviewProviders()
{
    for (auto& itemAndPreviewProvider : m_delayedItemPreviewProviders)
        itemAndPreviewProvider.provider(nil);
    m_delayedItemPreviewProviders.clear();
}

UITargetedDragPreview *DragDropInteractionState::previewForDragItem(UIDragItem *item, UIView *contentView, UIView *previewContainer) const
{
    auto foundSource = activeDragSourceForItem(item);
    if (!foundSource)
        return nil;

    auto& source = foundSource.value();
    if (shouldUseDragImageToCreatePreviewForDragSource(source)) {
        if (shouldUseVisiblePathToCreatePreviewForDragSource(source)) {
            auto path = source.visiblePath.value();
            UIBezierPath *visiblePath = [UIBezierPath bezierPathWithCGPath:path.ensurePlatformPath()];
            return createTargetedDragPreview(source.image.get(), contentView, previewContainer, source.dragPreviewFrameInRootViewCoordinates, { }, nil, visiblePath).autorelease();
        }
        return createTargetedDragPreview(source.image.get(), contentView, previewContainer, source.dragPreviewFrameInRootViewCoordinates, { }, nil, nil).autorelease();
    }

    if (shouldUseTextIndicatorToCreatePreviewForDragSource(source)) {
        auto indicator = source.indicatorData.value();
        auto textIndicatorImage = uiImageForImage(indicator.contentImage.get());
        return createTargetedDragPreview(textIndicatorImage.get(), contentView, previewContainer, indicator.textBoundingRectInRootViewCoordinates, indicator.textRectsInBoundingRectCoordinates, [UIColor colorWithCGColor:cachedCGColor(indicator.estimatedBackgroundColor)], nil).autorelease();
    }

    return nil;
}

void DragDropInteractionState::dragSessionWillDelaySetDownAnimation(dispatch_block_t completion)
{
    m_dragCancelSetDownBlock = completion;
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
        item.eventPositionInContentCoordinates,
        item.dragPreviewFrameInRootViewCoordinates,
        dragImage,
        item.image.indicatorData(),
        item.image.visiblePath(),
        item.title.isEmpty() ? nil : (NSString *)item.title,
        item.url.isEmpty() ? nil : (NSURL *)item.url,
        true, // We assume here that drag previews need to be updated until proven otherwise in updatePreviewsForActiveDragSources().
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
    m_stagedDragSource = WTF::nullopt;
}

void DragDropInteractionState::dragAndDropSessionsDidEnd()
{
    clearAllDelayedItemPreviewProviders();

    // If any of UIKit's completion blocks are still in-flight when the drag interaction ends, we need to ensure that they are still invoked
    // to prevent UIKit from getting into an inconsistent state.
    if (auto completionBlock = takeDragCancelSetDownBlock())
        completionBlock();

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
            dragItem.previewProvider = [title = retainPtr((NSString *)source.linkTitle), url = retainPtr((NSURL *)source.linkURL), center = source.adjustedOrigin] () -> UIDragPreview * {
                UIURLDragPreviewView *previewView = [UIURLDragPreviewView viewWithTitle:title.get() URL:url.get()];
                previewView.center = center;
                UIDragPreviewParameters *parameters = [[[UIDragPreviewParameters alloc] initWithTextLineRects:@[ [NSValue valueWithCGRect:previewView.bounds] ]] autorelease];
                return [[[UIDragPreview alloc] initWithView:previewView parameters:parameters] autorelease];
            };
        }
#if ENABLE(INPUT_TYPE_COLOR)
        else if (source.action.contains(DragSourceAction::Color)) {
            dragItem.previewProvider = [image = source.image] () -> UIDragPreview * {
                UIImageView *imageView = [[[UIImageView alloc] initWithImage:image.get()] autorelease];
                UIDragPreviewParameters *parameters = [[[UIDragPreviewParameters alloc] initWithTextLineRects:@[ [NSValue valueWithCGRect:[imageView bounds]] ]] autorelease];
                return [[[UIDragPreview alloc] initWithView:imageView parameters:parameters] autorelease];
            };
        }
#endif

        source.possiblyNeedsDragPreviewUpdate = false;
    }
}

} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
