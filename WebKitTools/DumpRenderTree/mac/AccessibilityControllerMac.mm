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
#import <WebKit/WebTypesInternal.h>
#import <wtf/Vector.h>
#import <wtf/RetainPtr.h>

static JSStringRef nsStringToJSStringRef(NSString* string);
static NSString* attributesOfElement(id accessibilityObject);

static NSString* descriptionOfValue(id valueObject, id focusedAccessibilityObject)
{
    if (!valueObject)
        return NULL;

    if ([valueObject isKindOfClass:[NSArray class]])
        return [NSString stringWithFormat:@"<array of size %d>", [(NSArray*)valueObject count]];

    if ([valueObject isKindOfClass:[NSNumber class]])
        return [(NSNumber*)valueObject stringValue];

    if ([valueObject isKindOfClass:[NSValue class]]) {
        NSString* type = [NSString stringWithCString:[valueObject objCType] encoding:NSASCIIStringEncoding];
        NSValue* value = (NSValue*)valueObject;
        if ([type rangeOfString:@"NSRect"].length > 0)
            return [NSString stringWithFormat:@"NSRect: %@", NSStringFromRect([value rectValue])];
        if ([type rangeOfString:@"NSPoint"].length > 0)
            return [NSString stringWithFormat:@"NSPoint: %@", NSStringFromPoint([value pointValue])];
        if ([type rangeOfString:@"NSSize"].length > 0)
            return [NSString stringWithFormat:@"NSSize: %@", NSStringFromSize([value sizeValue])];
        if ([type rangeOfString:@"NSRange"].length > 0)
            return [NSString stringWithFormat:@"NSRange: %@", NSStringFromRange([value rangeValue])];
    }

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

JSStringRef AccessibilityController::attributesOfLinkedUIElementsForFocusedElement()
{
    WebHTMLView* view = [[mainFrame frameView] documentView];
    id accessibilityObject = [view accessibilityFocusedUIElement];

    NSArray* linkedElements = [accessibilityObject accessibilityAttributeValue:NSAccessibilityLinkedUIElementsAttribute];
    NSMutableString* allElementString = [NSMutableString string];
    NSUInteger i = 0, count = [linkedElements count];
    for (; i < count; ++i) {
        NSString* attributes = attributesOfElement([linkedElements objectAtIndex:i]);
        [allElementString appendFormat:@"%@\n------------\n",attributes];
    }
    
    return nsStringToJSStringRef(allElementString);
}


JSStringRef AccessibilityController::allAttributesOfFocusedElement()
{
    WebHTMLView* view = [[mainFrame frameView] documentView];
    id accessibilityObject = [view accessibilityFocusedUIElement];
    
    NSString* attributes = attributesOfElement(accessibilityObject);
    return nsStringToJSStringRef(attributes);
}

static NSString* attributesOfElement(id accessibilityObject)
{
    NSArray* supportedAttributes = [accessibilityObject accessibilityAttributeNames];

    NSMutableString* attributesString = [NSMutableString string];
    for (NSUInteger i = 0; i < [supportedAttributes count]; ++i) {
        NSString* attribute = [supportedAttributes objectAtIndex:i];
        
        // Right now, position provides useless and screen-specific information, so we do not
        // want to include it for the sake of universally passing tests.
        if ([attribute isEqualToString:@"AXPosition"])
            continue;
        
        id valueObject = [accessibilityObject accessibilityAttributeValue:attribute];
        NSString* value = descriptionOfValue(valueObject, accessibilityObject);
        [attributesString appendFormat:@"%@: %@\n",attribute,value];
    }
    
    return attributesString;
}
        
static JSStringRef nsStringToJSStringRef(NSString* string)
{
    Vector<UniChar> buffer([string length]);
    [string getCharacters:buffer.data()];
    return JSStringCreateWithCharacters(buffer.data(), buffer.size());
}

static JSStringRef concatenateAttributeAndValue(NSString* attribute, NSString* value)
{
    Vector<UniChar> buffer([attribute length]);
    [attribute getCharacters:buffer.data()];
    buffer.append(':');
    buffer.append(' ');

    Vector<UniChar> valueBuffer([value length]);
    [value getCharacters:valueBuffer.data()];
    buffer.append(valueBuffer);

    return JSStringCreateWithCharacters(buffer.data(), buffer.size());
}

JSStringRef AccessibilityController::roleOfFocusedElement()
{
    WebHTMLView* view = [[mainFrame frameView] documentView];
    id accessibilityObject = [view accessibilityFocusedUIElement];
    NSString* role = descriptionOfValue([accessibilityObject accessibilityAttributeValue:@"AXRole"], accessibilityObject);
    return concatenateAttributeAndValue(@"AXRole", role);
}

JSStringRef AccessibilityController::titleOfFocusedElement()
{
    WebHTMLView* view = [[mainFrame frameView] documentView];
    id accessibilityObject = [view accessibilityFocusedUIElement];
    NSString* title = descriptionOfValue([accessibilityObject accessibilityAttributeValue:@"AXTitle"], accessibilityObject);
    return concatenateAttributeAndValue(@"AXTitle", title);
}

JSStringRef AccessibilityController::descriptionOfFocusedElement()
{
    WebHTMLView* view = [[mainFrame frameView] documentView];
    id accessibilityObject = [view accessibilityFocusedUIElement];
    id description = descriptionOfValue([accessibilityObject accessibilityAttributeValue:@"AXDescription"], accessibilityObject);
    return concatenateAttributeAndValue(@"AXDescription", description);
}
