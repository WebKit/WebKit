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

#import "config.h"
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

#ifdef BUILDING_ON_TIGER
#define NSAccessibilityValueDescriptionAttribute @"AXValueDescription"
#endif

@interface NSObject (WebKitAccessibilityArrayCategory)
- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute index:(NSUInteger)index maxCount:(NSUInteger)maxCount;
@end

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
        NSString* role = [focusedAccessibilityObject accessibilityAttributeValue:NSAccessibilityRoleAttribute];
        NSString* title = [focusedAccessibilityObject accessibilityAttributeValue:NSAccessibilityTitleAttribute];
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
        
        // accessibilityAttributeValue: can throw an if an attribute is not returned.
        // For DumpRenderTree's purpose, we should ignore those exceptions
        @try {
            id valueObject = [accessibilityObject accessibilityAttributeValue:attribute];
            NSString* value = descriptionOfValue(valueObject, accessibilityObject);
            [attributesString appendFormat:@"%@: %@\n", attribute, value];
        } @catch (NSException* e) { }
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

void AccessibilityUIElement::getChildrenWithRange(Vector<AccessibilityUIElement>& elementVector, unsigned location, unsigned length)
{
    NSArray* children = [m_element accessibilityArrayAttributeValues:NSAccessibilityChildrenAttribute index:location maxCount:length];
    convertNSArrayToVector(children, elementVector);
}

int AccessibilityUIElement::childrenCount()
{
    Vector<AccessibilityUIElement> children;
    getChildren(children);
    
    return children.size();
}

AccessibilityUIElement AccessibilityUIElement::elementAtPoint(int x, int y)
{
    id element = [m_element accessibilityHitTest:NSMakePoint(x, y)];
    if (!element)
        return nil;
    
    return AccessibilityUIElement(element); 
}

AccessibilityUIElement AccessibilityUIElement::getChildAtIndex(unsigned index)
{
    Vector<AccessibilityUIElement> children;
    getChildrenWithRange(children, index, 1);

    if (children.size() == 1)
        return children[0];
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
    NSString* role = descriptionOfValue([m_element accessibilityAttributeValue:NSAccessibilityRoleAttribute], m_element);
    return concatenateAttributeAndValue(@"AXRole", role);
}

JSStringRef AccessibilityUIElement::subrole()
{
    NSString* role = descriptionOfValue([m_element accessibilityAttributeValue:NSAccessibilitySubroleAttribute], m_element);
    return concatenateAttributeAndValue(@"AXSubrole", role);
}

JSStringRef AccessibilityUIElement::title()
{
    NSString* title = descriptionOfValue([m_element accessibilityAttributeValue:NSAccessibilityTitleAttribute], m_element);
    return concatenateAttributeAndValue(@"AXTitle", title);
}

JSStringRef AccessibilityUIElement::description()
{
    id description = descriptionOfValue([m_element accessibilityAttributeValue:NSAccessibilityDescriptionAttribute], m_element);
    return concatenateAttributeAndValue(@"AXDescription", description);
}

JSStringRef AccessibilityUIElement::stringValue()
{
    id description = descriptionOfValue([m_element accessibilityAttributeValue:NSAccessibilityValueAttribute], m_element);
    return concatenateAttributeAndValue(@"AXValue", description);
}

JSStringRef AccessibilityUIElement::language()
{
    id description = descriptionOfValue([m_element accessibilityAttributeValue:@"AXLanguage"], m_element);
    return concatenateAttributeAndValue(@"AXLanguage", description);
}

double AccessibilityUIElement::x()
{
    NSValue* positionValue = [m_element accessibilityAttributeValue:NSAccessibilityPositionAttribute];
    return static_cast<double>([positionValue pointValue].x);    
}

double AccessibilityUIElement::y()
{
    NSValue* positionValue = [m_element accessibilityAttributeValue:NSAccessibilityPositionAttribute];
    return static_cast<double>([positionValue pointValue].y);    
}

double AccessibilityUIElement::width()
{
    NSValue* sizeValue = [m_element accessibilityAttributeValue:NSAccessibilitySizeAttribute];
    return static_cast<double>([sizeValue sizeValue].width);
}

double AccessibilityUIElement::height()
{
    NSValue* sizeValue = [m_element accessibilityAttributeValue:NSAccessibilitySizeAttribute];
    return static_cast<double>([sizeValue sizeValue].height);
}

double AccessibilityUIElement::clickPointX()
{
    NSValue* positionValue = [m_element accessibilityAttributeValue:@"AXClickPoint"];
    return static_cast<double>([positionValue pointValue].x);        
}

double AccessibilityUIElement::clickPointY()
{
    NSValue* positionValue = [m_element accessibilityAttributeValue:@"AXClickPoint"];
    return static_cast<double>([positionValue pointValue].y);
}

double AccessibilityUIElement::intValue()
{
    id value = [m_element accessibilityAttributeValue:NSAccessibilityValueAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber*)value doubleValue]; 
    return 0.0f;
}

double AccessibilityUIElement::minValue()
{
    id value = [m_element accessibilityAttributeValue:NSAccessibilityMinValueAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber*)value doubleValue]; 
    return 0.0f;
}

double AccessibilityUIElement::maxValue()
{
    id value = [m_element accessibilityAttributeValue:NSAccessibilityMaxValueAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber*)value doubleValue]; 
    return 0.0;
}

JSStringRef AccessibilityUIElement::valueDescription()
{
    NSString* valueDescription = [m_element accessibilityAttributeValue:NSAccessibilityValueDescriptionAttribute];
    if ([valueDescription isKindOfClass:[NSString class]])
         return [valueDescription createJSStringRef];
    return 0;
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    id value = [m_element accessibilityAttributeValue:NSAccessibilityInsertionPointLineNumberAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber *)value intValue]; 
    return -1;
}

bool AccessibilityUIElement::isActionSupported(JSStringRef action)
{
    NSArray* actions = [m_element accessibilityActionNames];
    return [actions containsObject:[NSString stringWithJSStringRef:action]];
}

bool AccessibilityUIElement::isEnabled()
{
    id value = [m_element accessibilityAttributeValue:NSAccessibilityEnabledAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    return false;
}

bool AccessibilityUIElement::isRequired() const
{
    id value = [m_element accessibilityAttributeValue:@"AXRequired"];
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    return false;
}

bool AccessibilityUIElement::isSelected() const
{
    id value = [m_element accessibilityAttributeValue:NSAccessibilitySelectedAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    return false;
}

// parameterized attributes
int AccessibilityUIElement::lineForIndex(int index)
{
    id value = [m_element accessibilityAttributeValue:NSAccessibilityLineForIndexParameterizedAttribute forParameter:[NSNumber numberWithInt:index]];
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

JSStringRef AccessibilityUIElement::stringForRange(unsigned location, unsigned length)
{
    NSRange range = NSMakeRange(location, length);
    id string = [m_element accessibilityAttributeValue:NSAccessibilityStringForRangeParameterizedAttribute forParameter:[NSValue valueWithRange:range]];
    if (![string isKindOfClass:[NSString class]])
        return 0;
    
    return [string createJSStringRef];
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

void AccessibilityUIElement::increment()
{
    [m_element accessibilityPerformAction:NSAccessibilityIncrementAction];
}

void AccessibilityUIElement::decrement()
{
    [m_element accessibilityPerformAction:NSAccessibilityDecrementAction];
}

JSStringRef AccessibilityUIElement::accessibilityValue() const
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}
