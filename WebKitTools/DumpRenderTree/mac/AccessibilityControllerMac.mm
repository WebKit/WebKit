/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#import "DumpRenderTree.h"
#import "AccessibilityController.h"

#import <Foundation/Foundation.h>
#import <JavaScriptCore/JSStringRef.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebHTMLView.h>
#import <wtf/Vector.h>
#import <wtf/RetainPtr.h>

static NSString* descriptionOfValue(id valueObject, id focusedAccessibilityObject)
{
    if (!valueObject)
        return NULL;

    if ([valueObject isKindOfClass:[NSArray class]])
        return [NSString stringWithFormat:@"<array of size %d>", [(NSArray*)valueObject count]];

    // Strip absolute URL paths
    NSString* description = [valueObject description];
    NSRange range = [description rangeOfString:@"LayoutTests"];
    if (range.length)
        return [description substringFromIndex:range.location];

    // Strip pointer locations
    if ([description rangeOfString:@": 0x"].length) {
        NSString* role = [focusedAccessibilityObject accessibilityAttributeValue:@"AXRole"];
        NSString* title = [focusedAccessibilityObject accessibilityAttributeValue:@"AXTitle"];
        if ([title length])
            return [NSString stringWithFormat:@"<%@: '%@'>", role, title];
        return [NSString stringWithFormat:@"<%@>", role];
    }
    
    return [valueObject description];
}

JSStringRef AccessibilityController::dumpCurrentAttributes()
{
    WebHTMLView* view = [[mainFrame frameView] documentView];
    id accessibilityObject = [view accessibilityFocusedUIElement];
    NSArray* supportedAttributes = [accessibilityObject accessibilityAttributeNames];

    Vector<UniChar> buffer;
    for (NSUInteger i = 0; i < [supportedAttributes count]; ++i) {
        NSString* attribute = [supportedAttributes objectAtIndex:i];
        id valueObject = [accessibilityObject accessibilityAttributeValue:attribute];
        NSString* value = descriptionOfValue(valueObject, accessibilityObject);

        Vector<UniChar> attributeBuffer([attribute length]);
        [attribute getCharacters:attributeBuffer.data()];
        buffer.append(attributeBuffer);
        buffer.append(':');
        buffer.append(' ');

        Vector<UniChar> valueBuffer([value length]);
        [value getCharacters:valueBuffer.data()];
        buffer.append(valueBuffer);
        buffer.append('\n');
    }

    return JSStringCreateWithCharacters(buffer.data(), buffer.size());
}
