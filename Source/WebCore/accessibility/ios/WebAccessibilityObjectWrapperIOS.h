/*
 * Copyright (C) 2008, Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if ENABLE(ACCESSIBILITY) && PLATFORM(IOS_FAMILY)

#import "AXObjectCache.h"
#import "AccessibilityObject.h"
#import "WebAccessibilityObjectWrapperBase.h"
#import "WAKView.h"

@interface WAKView (iOSAccessibility)
- (BOOL)accessibilityIsIgnored;
@end

@interface WebAccessibilityObjectWrapper : WebAccessibilityObjectWrapperBase {
    // Cached data to avoid frequent re-computation.
    int m_isAccessibilityElement;
    uint64_t m_accessibilityTraitsFromAncestor;
}

- (id)accessibilityHitTest:(CGPoint)point;
- (AccessibilityObjectWrapper *)accessibilityPostProcessHitTest:(CGPoint)point;
- (BOOL)accessibilityCanFuzzyHitTest;

- (BOOL)isAccessibilityElement;
- (NSString *)accessibilityLabel;
- (CGRect)accessibilityFrame;
- (NSString *)accessibilityValue;

- (NSInteger)accessibilityElementCount;
- (id)accessibilityElementAtIndex:(NSInteger)index;
- (NSInteger)indexOfAccessibilityElement:(id)element;

- (BOOL)isAttachment;

- (void)postNotification:(NSString *)notificationName;
@end

#endif // ENABLE(ACCESSIBILITY) && PLATFORM(IOS_FAMILY)
