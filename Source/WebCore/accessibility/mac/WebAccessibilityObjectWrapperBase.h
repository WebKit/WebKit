/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebAccessibilityObjectWrapperBase_h
#define WebAccessibilityObjectWrapperBase_h

#include "AXIsolatedTree.h"
#include "AXIsolatedTreeNode.h"
#include "AccessibilityObject.h"
#include <CoreGraphics/CoreGraphics.h>
#include <wtf/RefPtr.h>
#include <wtf/Variant.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class AccessibilityObject;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
class AXIsolatedTreeNode;
#endif
struct AccessibilitySearchCriteria;
class IntRect;
class FloatPoint;
class HTMLTextFormControlElement;
class Path;
class VisiblePosition;
}

@interface WebAccessibilityObjectWrapperBase : NSObject {
    WebCore::AccessibilityObject* m_object;
    WebCore::AXID _identifier;
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    RefPtr<WebCore::AXIsolatedTreeNode> m_isolatedTreeNode;
#endif
}
 
- (id)initWithAccessibilityObject:(WebCore::AccessibilityObject*)axObject;

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
@property (nonatomic, readonly) RefPtr<WebCore::AXIsolatedTreeNode> isolatedTreeNode;
@property (nonatomic, assign) WebCore::AXIsolatedTreeID isolatedTreeIdentifier;
#endif

- (void)detach;

@property (nonatomic, assign) WebCore::AXID identifier;

- (WebCore::AccessibilityObject*)accessibilityObject;
- (BOOL)updateObjectBackingStore;

// This can be either an AccessibilityObject or an AXIsolatedTreeNode
- (WebCore::AccessibilityObjectInterface*)axBackingObject;

// These are pre-fixed with base so that AppKit does not end up calling into these directly (bypassing safety checks).
- (NSString *)baseAccessibilityTitle;
- (NSString *)baseAccessibilityDescription;
- (NSString *)baseAccessibilityHelpText;
- (NSArray<NSString *> *)baseAccessibilitySpeechHint;

- (NSString *)ariaLandmarkRoleDescription;

- (id)attachmentView;
// Used to inform an element when a notification is posted for it. Used by tests.
- (void)accessibilityPostedNotification:(NSString *)notificationName;
- (void)accessibilityPostedNotification:(NSString *)notificationName userInfo:(NSDictionary *)userInfo;

- (CGPathRef)convertPathToScreenSpace:(WebCore::Path &)path;

- (CGRect)convertRectToSpace:(WebCore::FloatRect &)rect space:(WebCore::AccessibilityConversionSpace)space;

// Math related functions
- (NSArray *)accessibilityMathPostscriptPairs;
- (NSArray *)accessibilityMathPrescriptPairs;

extern WebCore::AccessibilitySearchCriteria accessibilitySearchCriteriaForSearchPredicateParameterizedAttribute(const NSDictionary *);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
extern RetainPtr<NSArray> convertToNSArray(const Vector<RefPtr<WebCore::AXIsolatedTreeNode>>&);
#endif
extern RetainPtr<NSArray> convertToNSArray(const WebCore::AccessibilityObject::AccessibilityChildrenVector&);

#if PLATFORM(IOS_FAMILY)
- (id)_accessibilityWebDocumentView;
#endif

@end

#endif // WebAccessibilityObjectWrapperBase_h
