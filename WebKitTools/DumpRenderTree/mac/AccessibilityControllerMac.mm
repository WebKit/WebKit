/*
 * Copyright (C) 2008, 2009, 2010 Apple Inc. All Rights Reserved.
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
#import "DumpRenderTree.h"
#import "AccessibilityController.h"

#import "AccessibilityUIElement.h"
#import <Foundation/Foundation.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebHTMLView.h>

AccessibilityController::AccessibilityController()
{
}

AccessibilityController::~AccessibilityController()
{
}

AccessibilityUIElement AccessibilityController::elementAtPoint(int x, int y)
{
    id accessibilityObject = [[[mainFrame frameView] documentView] accessibilityHitTest:NSMakePoint(x, y)];
    return AccessibilityUIElement(accessibilityObject);    
}

AccessibilityUIElement AccessibilityController::focusedElement()
{
    // FIXME: we could do some caching here.
    id accessibilityObject = [[[mainFrame frameView] documentView] accessibilityFocusedUIElement];
    return AccessibilityUIElement(accessibilityObject);
}

AccessibilityUIElement AccessibilityController::rootElement()
{
    // FIXME: we could do some caching here.
    id accessibilityObject = [[mainFrame frameView] documentView];
    return AccessibilityUIElement(accessibilityObject);
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

void AccessibilityController::addNotificationListener(PlatformUIElement, JSObjectRef)
{
}

void AccessibilityController::notificationReceived(PlatformUIElement, const std::string&)
{
}
