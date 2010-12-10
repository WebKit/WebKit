/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(SMOOTH_SCROLLING)

#include "ScrollAnimatorMac.h"
#include "ScrollbarClient.h"

@interface NSObject (NSScrollAnimationHelperDetails)
- (id)initWithDelegate:(id)delegate;
- (void)_stopRun;
- (BOOL)_isAnimating;
- (NSPoint)targetOrigin;
@end

@interface ScrollAnimationHelperDelegate : NSObject
{
    WebCore::ScrollAnimatorMac* _animator;
}

- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorMac*)scrollAnimator;

- (NSRect)bounds;
- (void)_immediateScrollToPoint:(NSPoint)newPosition;
- (NSSize)convertSizeToBase:(NSSize)size;
- (NSSize)convertSizeFromBase:(NSSize)size;

- (id)superview; // Return nil.
- (id)documentView; // Return nil.
- (id)window; // Return nil.
- (void)_recursiveRecomputeToolTips; // No-op.
@end

static NSSize abs(NSSize size)
{
    NSSize finalSize = size;
    if (finalSize.width < 0)
        finalSize.width = -finalSize.width;
    if (finalSize.height < 0)
        finalSize.height = -finalSize.height;
    return finalSize;    
}

@implementation ScrollAnimationHelperDelegate

- (id)initWithScrollAnimator:(WebCore::ScrollAnimatorMac*)scrollAnimator
{
    self = [super init];
    if (!self)
        return nil;

    _animator = scrollAnimator;
    return self;
}

- (NSRect)bounds
{
    WebCore::FloatPoint currentPosition = _animator->currentPosition();
    return NSMakeRect(currentPosition.x(), currentPosition.y(), 0, 0);
}

- (void)_immediateScrollToPoint:(NSPoint)newPosition
{
    _animator->immediateScrollToPoint(newPosition);
}

- (NSSize)convertSizeToBase:(NSSize)size
{
    return abs(size);
}

- (NSSize)convertSizeFromBase:(NSSize)size
{
    return abs(size);
}

- (id)superview
{
    return nil;
}

- (id)documentView
{
    return nil;
}

- (id)window
{
    return nil;
}

- (void)_recursiveRecomputeToolTips
{
}

@end

namespace WebCore {

ScrollAnimator* ScrollAnimator::create(ScrollbarClient* client)
{
    return new ScrollAnimatorMac(client);
}

ScrollAnimatorMac::ScrollAnimatorMac(ScrollbarClient* client)
    : ScrollAnimator(client)
{
    m_scrollAnimationHelperDelegate.adoptNS([[ScrollAnimationHelperDelegate alloc] initWithScrollAnimator:this]);
    m_scrollAnimationHelper.adoptNS([[NSClassFromString(@"NSScrollAnimationHelper") alloc] initWithDelegate:m_scrollAnimationHelperDelegate.get()]);
}

ScrollAnimatorMac::~ScrollAnimatorMac()
{
}

bool ScrollAnimatorMac::scroll(ScrollbarOrientation orientation, ScrollGranularity granularity, float step, float multiplier)
{
    if (![[NSUserDefaults standardUserDefaults] boolForKey:@"AppleScrollAnimationEnabled"])
        return ScrollAnimator::scroll(orientation, granularity, step, multiplier);

    if (granularity == ScrollByPixel)
        return ScrollAnimator::scroll(orientation, granularity, step, multiplier);

    float currentPos = orientation == HorizontalScrollbar ? m_currentPosX : m_currentPosY;
    float newPos = std::max<float>(std::min<float>(currentPos + (step * multiplier), static_cast<float>(m_client->scrollSize(orientation))), 0);
    if (currentPos == newPos)
        return false;

    NSPoint newPoint;
    if ([m_scrollAnimationHelper.get() _isAnimating]) {
        NSPoint targetOrigin = [m_scrollAnimationHelper.get() targetOrigin];
        newPoint = orientation == HorizontalScrollbar ? NSMakePoint(newPos, targetOrigin.y) : NSMakePoint(targetOrigin.x, newPos);
    } else
        newPoint = orientation == HorizontalScrollbar ? NSMakePoint(newPos, m_currentPosY) : NSMakePoint(m_currentPosX, newPos);

    [m_scrollAnimationHelper.get() scrollToPoint:newPoint];
    return true;
}

void ScrollAnimatorMac::setScrollPositionAndStopAnimation(ScrollbarOrientation orientation, float pos)
{
    [m_scrollAnimationHelper.get() _stopRun];
    ScrollAnimator::setScrollPositionAndStopAnimation(orientation, pos);
}

FloatPoint ScrollAnimatorMac::currentPosition() const
{
    return FloatPoint(m_currentPosX, m_currentPosY);
}

void ScrollAnimatorMac::immediateScrollToPoint(const FloatPoint& newPosition)
{
    m_currentPosX = newPosition.x();
    m_currentPosY = newPosition.y();

    m_client->setScrollOffsetFromAnimation(IntPoint(m_currentPosX, m_currentPosY));
}

} // namespace WebCore

#endif // ENABLE(SMOOTH_SCROLLING)
