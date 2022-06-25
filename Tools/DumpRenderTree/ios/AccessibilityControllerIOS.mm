/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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

#import "config.h"
#import "AccessibilityController.h"

#import "AccessibilityCommonMac.h"
#import "AccessibilityNotificationHandler.h"
#import "AccessibilityUIElement.h"
#import "DumpRenderTree.h"
#import "JSBasics.h"
#import <Foundation/Foundation.h>
#import <WebKit/WebFramePrivate.h>
#import <WebKit/WebHTMLViewPrivate.h>

@interface WebHTMLView (Private)
- (id)accessibilityFocusedUIElement;
@end

AccessibilityController::AccessibilityController()
{
}

AccessibilityController::~AccessibilityController()
{
    // ResetToConsistentState should have cleared this already.
    ASSERT(!m_globalNotificationHandler);
}

AccessibilityUIElement AccessibilityController::elementAtPoint(int x, int y)
{
    return rootElement().elementAtPoint(x, y);
}

AccessibilityUIElement AccessibilityController::focusedElement()
{
    id webDocumentView = [[mainFrame frameView] documentView];
    if ([webDocumentView isKindOfClass:[WebHTMLView class]])
        return AccessibilityUIElement([(WebHTMLView *)webDocumentView accessibilityFocusedUIElement]);
    return 0;
}

AccessibilityUIElement AccessibilityController::rootElement()
{
    // FIXME: we could do some caching here.
    id webDocumentView = [[mainFrame frameView] documentView];
    if ([webDocumentView isKindOfClass:[WebHTMLView class]])
        return AccessibilityUIElement([(WebHTMLView *)webDocumentView accessibilityRootElement]);
    return 0;
}

static id findAccessibleObjectById(id obj, NSString *idAttribute)
{
    id objIdAttribute = [obj accessibilityIdentifier];
    if ([objIdAttribute isKindOfClass:[NSString class]] && [objIdAttribute isEqualToString:idAttribute])
        return obj;
    
    NSUInteger childrenCount = [obj accessibilityElementCount];
    for (NSUInteger i = 0; i < childrenCount; ++i) {
        id result = findAccessibleObjectById([obj accessibilityElementAtIndex:i], idAttribute);
        if (result)
            return result;
    }
    
    return 0;
}

AccessibilityUIElement AccessibilityController::accessibleElementById(JSStringRef idAttributeRef)
{
    id webDocumentView = [[mainFrame frameView] documentView];
    if (![webDocumentView isKindOfClass:[WebHTMLView class]])
        return 0;
    
    id root = [(WebHTMLView *)webDocumentView accessibilityRootElement];
    NSString *idAttribute = [NSString stringWithJSStringRef:idAttributeRef];
    id result = findAccessibleObjectById(root, idAttribute);
    if (result)
        return AccessibilityUIElement(result);
    
    return 0;
}

void AccessibilityController::setLogFocusEvents(bool)
{
}

void AccessibilityController::setLogScrollingStartEvents(bool)
{
}

void AccessibilityController::setLogValueChangeEvents(bool)
{
}

void AccessibilityController::setLogAccessibilityEvents(bool)
{
}

bool AccessibilityController::addNotificationListener(JSObjectRef functionCallback)
{
    if (!functionCallback)
        return false;
    
    // Mac programmers should not be adding more than one global notification listener.
    // Other platforms may be different.
    if (m_globalNotificationHandler)
        return false;
    m_globalNotificationHandler = adoptNS([[AccessibilityNotificationHandler alloc] init]);
    [m_globalNotificationHandler setCallback:functionCallback];
    [m_globalNotificationHandler startObserving];
    
    return true;
}

void AccessibilityController::platformResetToConsistentState()
{
    [m_globalNotificationHandler stopObserving];
    m_globalNotificationHandler.clear();
}

void AccessibilityController::removeNotificationListener()
{
}

void AccessibilityController::enableEnhancedAccessibility(bool enable)
{
    [mainFrame setEnhancedAccessibility:enable];
}

bool AccessibilityController::enhancedAccessibilityEnabled()
{
    return [mainFrame enhancedAccessibilityEnabled];
}

JSRetainPtr<JSStringRef> AccessibilityController::platformName() const
{
    return WTR::createJSString("ios");
}
