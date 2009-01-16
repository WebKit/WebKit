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
#import "AccessibilityUIElement.h"

#import <Foundation/Foundation.h>
#import <JavaScriptCore/JSStringRef.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebHTMLView.h>
#import <WebKit/WebTypesInternal.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

AccessibilityUIElement::AccessibilityUIElement(PlatformUIElement element)
    : m_element(element)
{
    [m_element retain];
}

AccessibilityUIElement::AccessibilityUIElement(const AccessibilityUIElement& other)
    : m_element(other.m_element)
{
    [m_element retain];
}

AccessibilityUIElement::~AccessibilityUIElement()
{
    [m_element release];
}

@interface NSString (JSStringRefAdditions)
+ (NSString *)stringWithJSStringRef:(JSStringRef)jsStringRef;
- (JSStringRef)createJSStringRef;
@end

@implementation NSString (JSStringRefAdditions)

+ (NSString *)stringWithJSStringRef:(JSStringRef)jsStringRef
{
    if (!jsStringRef)
        return NULL;
    
    CFStringRef cfString = JSStringCopyCFString(kCFAllocatorDefault, jsStringRef);
    return [(NSString *)cfString autorelease];
}

- (JSStringRef)createJSStringRef
{
    return JSStringCreateWithCFString((CFStringRef)self);
}

@end

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
    if ([description rangeOfString:@"0x"].length) {
        NSString* role = [focusedAccessibilityObject accessibilityAttributeValue:@"AXRole"];
        NSString* title = [focusedAccessibilityObject accessibilityAttributeValue:@"AXTitle"];
        if ([title length])
            return [NSString stringWithFormat:@"<%@: '%@'>", role, title];
        return [NSString stringWithFormat:@"<%@>", role];
    }
    
    return [valueObject description];
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
        [attributesString appendFormat:@"%@: %@\n", attribute, value];
    }
    
    return attributesString;
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

static void convertNSArrayToVector(NSArray* array, Vector<AccessibilityUIElement>& elementVector)
{
    NSUInteger count = [array count];
    for (NSUInteger i = 0; i < count; ++i)
        elementVector.append(AccessibilityUIElement([array objectAtIndex:i]));
}

static JSStringRef descriptionOfElements(Vector<AccessibilityUIElement>& elementVector)
{
    NSMutableString* allElementString = [NSMutableString string];
    size_t size = elementVector.size();
    for (size_t i = 0; i < size; ++i) {
        NSString* attributes = attributesOfElement(elementVector[i].platformUIElement());
        [allElementString appendFormat:@"%@\n------------\n", attributes];
    }
    
    return [allElementString createJSStringRef];
}

void AccessibilityUIElement::getLinkedUIElements(Vector<AccessibilityUIElement>& elementVector)
{
    NSArray* linkedElements = [m_element accessibilityAttributeValue:NSAccessibilityLinkedUIElementsAttribute];
    convertNSArrayToVector(linkedElements, elementVector);
}

void AccessibilityUIElement::getDocumentLinks(Vector<AccessibilityUIElement>& elementVector)
{
    NSArray* linkElements = [m_element accessibilityAttributeValue:@"AXLinkUIElements"];
    convertNSArrayToVector(linkElements, elementVector);
}

void AccessibilityUIElement::getChildren(Vector<AccessibilityUIElement>& elementVector)
{
    NSArray* children = [m_element accessibilityAttributeValue:NSAccessibilityChildrenAttribute];
    convertNSArrayToVector(children, elementVector);
}

AccessibilityUIElement AccessibilityUIElement::getChildAtIndex(unsigned index)
{
    Vector<AccessibilityUIElement> children;
    getChildren(children);

    if (index < children.size())
        return children[index];
    return nil;
}

AccessibilityUIElement AccessibilityUIElement::titleUIElement()
{
    id accessibilityObject = [m_element accessibilityAttributeValue:NSAccessibilityTitleUIElementAttribute];
    if (accessibilityObject)
        return AccessibilityUIElement(accessibilityObject);
    
    return nil;
}

AccessibilityUIElement AccessibilityUIElement::parentElement()
{
    id accessibilityObject = [m_element accessibilityAttributeValue:NSAccessibilityParentAttribute];
    if (accessibilityObject)
        return AccessibilityUIElement(accessibilityObject);
    
    return nil;
}

JSStringRef AccessibilityUIElement::attributesOfLinkedUIElements()
{
    Vector<AccessibilityUIElement> linkedElements;
    getLinkedUIElements(linkedElements);
    return descriptionOfElements(linkedElements);
}

JSStringRef AccessibilityUIElement::attributesOfDocumentLinks()
{
    Vector<AccessibilityUIElement> linkElements;
    getDocumentLinks(linkElements);
    return descriptionOfElements(linkElements);
}

JSStringRef AccessibilityUIElement::attributesOfChildren()
{
    Vector<AccessibilityUIElement> children;
    getChildren(children);
    return descriptionOfElements(children);
}

JSStringRef AccessibilityUIElement::allAttributes()
{
    NSString* attributes = attributesOfElement(m_element);
    return [attributes createJSStringRef];
}

JSStringRef AccessibilityUIElement::attributeValue(JSStringRef attribute)
{
    id value = [m_element accessibilityAttributeValue:[NSString stringWithJSStringRef:attribute]];
    if (![value isKindOfClass:[NSString class]])
        return NULL;
    return [value createJSStringRef];
}

bool AccessibilityUIElement::isAttributeSettable(JSStringRef attribute)
{
    return [m_element accessibilityIsAttributeSettable:[NSString stringWithJSStringRef:attribute]];
}

JSStringRef AccessibilityUIElement::parameterizedAttributeNames()
{
    NSArray* supportedParameterizedAttributes = [m_element accessibilityParameterizedAttributeNames];
    
    NSMutableString* attributesString = [NSMutableString string];
    for (NSUInteger i = 0; i < [supportedParameterizedAttributes count]; ++i) {
        [attributesString appendFormat:@"%@\n", [supportedParameterizedAttributes objectAtIndex:i]];
    }
    
    return [attributesString createJSStringRef];
}

JSStringRef AccessibilityUIElement::role()
{
    NSString* role = descriptionOfValue([m_element accessibilityAttributeValue:@"AXRole"], m_element);
    return concatenateAttributeAndValue(@"AXRole", role);
}

JSStringRef AccessibilityUIElement::title()
{
    NSString* title = descriptionOfValue([m_element accessibilityAttributeValue:@"AXTitle"], m_element);
    return concatenateAttributeAndValue(@"AXTitle", title);
}

JSStringRef AccessibilityUIElement::description()
{
    id description = descriptionOfValue([m_element accessibilityAttributeValue:@"AXDescription"], m_element);
    return concatenateAttributeAndValue(@"AXDescription", description);
}

double AccessibilityUIElement::width()
{
    NSValue* sizeValue = [m_element accessibilityAttributeValue:@"AXSize"];
    return static_cast<double>([sizeValue sizeValue].width);
}

double AccessibilityUIElement::height()
{
    NSValue* sizeValue = [m_element accessibilityAttributeValue:@"AXSize"];
    return static_cast<double>([sizeValue sizeValue].height);
}

double AccessibilityUIElement::intValue()
{
    id value = [m_element accessibilityAttributeValue:@"AXValue"];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber*)value doubleValue]; 
    return 0.0f;
}

double AccessibilityUIElement::minValue()
{
    id value = [m_element accessibilityAttributeValue:@"AXMinValue"];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber*)value doubleValue]; 
    return 0.0f;
}

double AccessibilityUIElement::maxValue()
{
    id value = [m_element accessibilityAttributeValue:@"AXMaxValue"];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber*)value doubleValue]; 
    return 0.0;
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    id value = [m_element accessibilityAttributeValue:@"AXInsertionPointLineNumber"];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber *)value intValue]; 
    return -1;
}

bool AccessibilityUIElement::supportsPressAction()
{
    NSArray* actions = [m_element accessibilityActionNames];
    return [actions containsObject:@"AXPress"];
}

// parameterized attributes
int AccessibilityUIElement::lineForIndex(int index)
{
    id value = [m_element accessibilityAttributeValue:@"AXLineForIndex" forParameter:[NSNumber numberWithInt:index]];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber *)value intValue]; 
    return -1;
}

JSStringRef AccessibilityUIElement::boundsForRange(unsigned location, unsigned length)
{
    NSRange range = NSMakeRange(location, length);
    id value = [m_element accessibilityAttributeValue:NSAccessibilityBoundsForRangeParameterizedAttribute forParameter:[NSValue valueWithRange:range]];
    NSRect rect = NSMakeRect(0,0,0,0);
    if ([value isKindOfClass:[NSValue class]])
        rect = [value rectValue]; 
    
    // don't return position information because it is platform dependent
    NSMutableString* boundsDescription = [NSMutableString stringWithFormat:@"{{%f, %f}, {%f, %f}}",-1.0f,-1.0f,rect.size.width,rect.size.height];
    return [boundsDescription createJSStringRef];
}

JSStringRef AccessibilityUIElement::attributesOfColumnHeaders()
{
    // not yet defined in AppKit... odd
    NSArray* columnHeadersArray = [m_element accessibilityAttributeValue:@"AXColumnHeaderUIElements"];
    Vector<AccessibilityUIElement> columnHeadersVector;
    convertNSArrayToVector(columnHeadersArray, columnHeadersVector);
    return descriptionOfElements(columnHeadersVector);
}

JSStringRef AccessibilityUIElement::attributesOfRowHeaders()
{
    NSArray* rowHeadersArray = [m_element accessibilityAttributeValue:@"AXRowHeaderUIElements"];
    Vector<AccessibilityUIElement> rowHeadersVector;
    convertNSArrayToVector(rowHeadersArray, rowHeadersVector);
    return descriptionOfElements(rowHeadersVector);
}

JSStringRef AccessibilityUIElement::attributesOfColumns()
{
    NSArray* columnsArray = [m_element accessibilityAttributeValue:NSAccessibilityColumnsAttribute];
    Vector<AccessibilityUIElement> columnsVector;
    convertNSArrayToVector(columnsArray, columnsVector);
    return descriptionOfElements(columnsVector);
}

JSStringRef AccessibilityUIElement::attributesOfRows()
{
    NSArray* rowsArray = [m_element accessibilityAttributeValue:NSAccessibilityRowsAttribute];
    Vector<AccessibilityUIElement> rowsVector;
    convertNSArrayToVector(rowsArray, rowsVector);
    return descriptionOfElements(rowsVector);
}

JSStringRef AccessibilityUIElement::attributesOfVisibleCells()
{
    NSArray* cellsArray = [m_element accessibilityAttributeValue:@"AXVisibleCells"];
    Vector<AccessibilityUIElement> cellsVector;
    convertNSArrayToVector(cellsArray, cellsVector);
    return descriptionOfElements(cellsVector);
}

JSStringRef AccessibilityUIElement::attributesOfHeader()
{
    id headerObject = [m_element accessibilityAttributeValue:NSAccessibilityHeaderAttribute];
    if (!headerObject)
        return [@"" createJSStringRef];
    
    Vector<AccessibilityUIElement> headerVector;
    headerVector.append(headerObject);
    return descriptionOfElements(headerVector);
}

int AccessibilityUIElement::indexInTable()
{
    NSNumber* indexNumber = [m_element accessibilityAttributeValue:NSAccessibilityIndexAttribute];
    if (!indexNumber)
        return -1;
    return [indexNumber intValue];
}

JSStringRef AccessibilityUIElement::rowIndexRange()
{
    NSValue* indexRange = [m_element accessibilityAttributeValue:@"AXRowIndexRange"];
    NSRange range = indexRange ? [indexRange rangeValue] : NSMakeRange(0,0);
    NSMutableString* rangeDescription = [NSMutableString stringWithFormat:@"{%d, %d}",range.location, range.length];
    return [rangeDescription createJSStringRef];
}

JSStringRef AccessibilityUIElement::columnIndexRange()
{
    NSNumber* indexRange = [m_element accessibilityAttributeValue:@"AXColumnIndexRange"];
    NSRange range = indexRange ? [indexRange rangeValue] : NSMakeRange(0,0);
    NSMutableString* rangeDescription = [NSMutableString stringWithFormat:@"{%d, %d}",range.location, range.length];
    return [rangeDescription createJSStringRef];    
}

AccessibilityUIElement AccessibilityUIElement::cellForColumnAndRow(unsigned col, unsigned row)
{
    NSArray *colRowArray = [NSArray arrayWithObjects:[NSNumber numberWithUnsignedInt:col], [NSNumber numberWithUnsignedInt:row], nil];
    return [m_element accessibilityAttributeValue:@"AXCellForColumnAndRow" forParameter:colRowArray];
}

JSStringRef AccessibilityUIElement::selectedTextRange()
{
    NSNumber *indexRange = [m_element accessibilityAttributeValue:NSAccessibilitySelectedTextRangeAttribute];
    NSRange range = indexRange ? [indexRange rangeValue] : NSMakeRange(0,0);
    NSMutableString *rangeDescription = [NSMutableString stringWithFormat:@"{%d, %d}",range.location, range.length];
    return [rangeDescription createJSStringRef];    
}

void AccessibilityUIElement::setSelectedTextRange(unsigned location, unsigned length)
{
    NSRange textRange = NSMakeRange(location, length);
    NSValue *textRangeValue = [NSValue valueWithRange:textRange];
    [m_element accessibilitySetValue:textRangeValue forAttribute:NSAccessibilitySelectedTextRangeAttribute];
}
