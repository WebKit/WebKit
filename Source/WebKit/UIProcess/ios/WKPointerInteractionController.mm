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
#import "WKPointerInteractionController.h"

#if HAVE(UI_POINTER_INTERACTION)

#import "InteractionInformationAtPosition.h"
#import "InteractionInformationRequest.h"
#import "WKContentView.h"
#import "WKWebViewIOS.h"
#import <WebCore/FloatRect.h>
#import <WebCore/IntRect.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

static NSString * const pointerRegionIdentifier = @"WKPointerRegion";
static NSString * const editablePointerRegionIdentifier = @"WKEditablePointerRegion";

@implementation WKPointerInteractionController {
    WeakObjCPtr<WKContentView<WKPointerInteractionControllerDelegate>> _contentView;
    RetainPtr<UIPointerInteraction> _pointerInteraction;
    RetainPtr<UIPointerRegion> _lastPointerRegion;
    BOOL _pointerInteractionRegionNeedsUpdate;
}

- (instancetype)initWithContentView:(WKContentView<WKPointerInteractionControllerDelegate> *)contentView
{
    if (!(self = [super init]))
        return nil;

    _contentView = contentView;
    return self;
}

- (UIPointerInteraction *)pointerInteraction
{
    return _pointerInteraction.get();
}

- (void)setUpPointerInteraction
{
    _pointerInteraction = adoptNS([[UIPointerInteraction alloc] initWithDelegate:_contentView.get().get()]);
}

- (void)cleanUp
{
    _pointerInteraction = nil;
    _pointerInteractionRegionNeedsUpdate = NO;
}

- (UIPointerRegion *)regionForRequest:(UIPointerRegionRequest *)request
{
    [self _updateLastPointerRegionIfNeeded:request];
    return _lastPointerRegion.get();
}

- (void)_updateLastPointerRegionIfNeeded:(UIPointerRegionRequest *)request
{
    RetainPtr contentView = _contentView.get();
    WebKit::InteractionInformationRequest interactionInformationRequest;
    interactionInformationRequest.point = WebCore::roundedIntPoint(request.location);
    interactionInformationRequest.includeCursorContext = true;
    interactionInformationRequest.includeHasDoubleClickHandler = false;

    if ([contentView _currentPositionInformationIsValidForRequest:interactionInformationRequest]) {
        auto& positionInformation = [contentView positionInformation];
        _lastPointerRegion = [self pointerRegionForPositionInformation:positionInformation point:request.location];
        [[contentView webView] _setPointerTouchCompatibilitySimulatorEnabled:positionInformation.needsPointerTouchCompatibilityQuirk];
        _pointerInteractionRegionNeedsUpdate = NO;
        return;
    }

    // Note: In this case where position information is not up-to-date, checking the last cached
    // position information may not yield expected results, since the client (or any other code in
    // WebKit) may have queried position information at a totally different location, for unrelated
    // reasons. However, this seems to work well in practice, since pointer updates are very frequent,
    // so any invalid state would only persist for a single frame.
    if ([contentView webView]._editable && ![contentView positionInformation].cursorContext.shouldNotUseIBeamInEditableContent) {
        auto lastRegionRect = _lastPointerRegion ? [_lastPointerRegion rect] : [contentView bounds];
        _lastPointerRegion = [UIPointerRegion regionWithRect:lastRegionRect identifier:editablePointerRegionIdentifier];
    }

    if (_pointerInteractionRegionNeedsUpdate)
        return;

    _pointerInteractionRegionNeedsUpdate = YES;

    [contentView doAfterPositionInformationUpdate:[weakSelf = WeakObjCPtr<WKPointerInteractionController>(self), location = request.location](WebKit::InteractionInformationAtPosition information) {
        auto strongSelf = weakSelf.get();
        if (!strongSelf)
            return;

        strongSelf->_pointerInteractionRegionNeedsUpdate = NO;
        strongSelf->_lastPointerRegion = [strongSelf pointerRegionForPositionInformation:information point:location];
        [[strongSelf->_contentView.get() webView] _setPointerTouchCompatibilitySimulatorEnabled:information.needsPointerTouchCompatibilityQuirk];
        [strongSelf->_pointerInteraction invalidate];
    } forRequest:interactionInformationRequest];
}

- (UIPointerRegion *)pointerRegionForPositionInformation:(const WebKit::InteractionInformationAtPosition&)interactionInformation point:(CGPoint)location
{
    WebCore::FloatRect expandedLineRect = enclosingIntRect(interactionInformation.cursorContext.lineCaretExtent);

    // Pad lines of text in order to avoid switching back to the dot cursor between lines.
    // This matches the value that UIKit uses.
    // FIXME: Should this account for vertical writing mode?
    expandedLineRect.inflateY(10);

    if (auto cursor = interactionInformation.cursorContext.cursor) {
        auto cursorType = cursor->type();
        if (cursorType == WebCore::Cursor::Type::Hand)
            return [UIPointerRegion regionWithRect:interactionInformation.bounds identifier:pointerRegionIdentifier];

        if (cursorType == WebCore::Cursor::Type::IBeam && expandedLineRect.contains(location))
            return [UIPointerRegion regionWithRect:expandedLineRect identifier:pointerRegionIdentifier];
    }

    RetainPtr contentView = _contentView.get();
    if ([contentView webView]._editable) {
        if (expandedLineRect.contains(location))
            return [UIPointerRegion regionWithRect:expandedLineRect identifier:pointerRegionIdentifier];
        return [UIPointerRegion regionWithRect:[contentView bounds] identifier:editablePointerRegionIdentifier];
    }

    return [UIPointerRegion regionWithRect:[contentView bounds] identifier:pointerRegionIdentifier];
}

- (UIPointerStyle *)styleForRegion:(UIPointerRegion *)region
{
    RetainPtr contentView = _contentView.get();
    if (!contentView)
        return [UIPointerStyle systemPointerStyle];

    auto& positionInformation = [contentView positionInformation];
    double scaleFactor = [contentView _contentZoomScale];

    auto lineCaretExtent = positionInformation.cursorContext.lineCaretExtent;
    UIPointerStyle *(^iBeamCursor)(void) = ^{
        bool isVertical = positionInformation.cursorContext.isVerticalWritingMode;
        float beamLength = (isVertical ? lineCaretExtent.width() : lineCaretExtent.height()) * scaleFactor;
        auto axisOrientation = isVertical ? UIAxisHorizontal : UIAxisVertical;
        auto iBeamConstraintAxes = isVertical ? UIAxisHorizontal : UIAxisVertical;
        auto regionLengthInBlockAxis = isVertical ? CGRectGetWidth(region.rect) : CGRectGetHeight(region.rect);

        // If the I-beam is so large that the magnetism is hard to fight, we should not apply any magnetism.
        static constexpr auto maximumBeamSnappingLength = 100;
        if (beamLength > maximumBeamSnappingLength || regionLengthInBlockAxis > maximumBeamSnappingLength)
            iBeamConstraintAxes = UIAxisNeither;

        // If the region is the size of the view, we should not apply any magnetism.
        if ([region.identifier isEqual:editablePointerRegionIdentifier])
            iBeamConstraintAxes = UIAxisNeither;

        return [UIPointerStyle styleWithShape:[UIPointerShape beamWithPreferredLength:beamLength axis:axisOrientation] constrainedAxes:iBeamConstraintAxes];
    };

    if ([contentView webView]._editable) {
        if (positionInformation.cursorContext.shouldNotUseIBeamInEditableContent)
            return [UIPointerStyle systemPointerStyle];
        return iBeamCursor();
    }

    if (auto cursor = positionInformation.cursorContext.cursor; cursor && [region.identifier isEqual:pointerRegionIdentifier]) {
        auto cursorType = cursor->type();
        if (cursorType == WebCore::Cursor::Type::Hand)
            return [UIPointerStyle systemPointerStyle];

        if (cursorType == WebCore::Cursor::Type::IBeam && lineCaretExtent.contains(positionInformation.request.point))
            return iBeamCursor();
    }

    return [UIPointerStyle systemPointerStyle];
}

@end

#endif // HAVE(UI_POINTER_INTERACTION)
