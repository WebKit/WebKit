/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#import "AccessibilityUIElement.h"

#import "AccessibilityCommonMac.h"
#import "AccessibilityNotificationHandler.h"
#import "DumpRenderTree.h"
#import "JSBasics.h"
#import <Foundation/Foundation.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebHTMLView.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/VectorCocoa.h>

#ifndef NSAccessibilityDOMIdentifierAttribute
#define NSAccessibilityDOMIdentifierAttribute @"AXDOMIdentifier"
#endif

#ifndef NSAccessibilityOwnsAttribute
#define NSAccessibilityOwnsAttribute @"AXOwns"
#endif

#ifndef NSAccessibilityGrabbedAttribute
#define NSAccessibilityGrabbedAttribute @"AXGrabbed"
#endif

#ifndef NSAccessibilityDropEffectsAttribute
#define NSAccessibilityDropEffectsAttribute @"AXDropEffects"
#endif

#ifndef NSAccessibilityPathAttribute
#define NSAccessibilityPathAttribute @"AXPath"
#endif

// Text
#ifndef NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute
#define NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute @"AXEndTextMarkerForBounds"
#endif

#ifndef NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute
#define NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute @"AXStartTextMarkerForBounds"
#endif

#ifndef NSAccessibilitySelectedTextMarkerRangeAttribute
#define NSAccessibilitySelectedTextMarkerRangeAttribute @"AXSelectedTextMarkerRange"
#endif

typedef void (*AXPostedNotificationCallback)(id element, NSString* notification, void* context);

@interface NSObject (WebKitAccessibilityAdditions)
- (BOOL)accessibilityReplaceRange:(NSRange)range withText:(NSString *)string;
- (BOOL)accessibilityInsertText:(NSString *)text;
- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute index:(NSUInteger)index maxCount:(NSUInteger)maxCount;
- (NSUInteger)accessibilityIndexOfChild:(id)child;
- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute;
- (void)_accessibilityScrollToMakeVisibleWithSubFocus:(NSRect)rect;
- (void)_accessibilityScrollToGlobalPoint:(NSPoint)point;
- (void)_accessibilitySetValue:(id)value forAttribute:(NSString*)attributeName;
@end

static std::optional<AccessibilityUIElement> makeVectorElement(const AccessibilityUIElement*, id element)
{
    return { { element } };
}

static std::optional<AccessibilityTextMarkerRange> makeVectorElement(const AccessibilityTextMarkerRange*, id element)
{
    return { { element } };
}

AccessibilityUIElement::AccessibilityUIElement(id element)
    : m_element(element)
{
}

static NSString* descriptionOfValue(id valueObject, id focusedAccessibilityObject)
{
    if (!valueObject)
        return NULL;

    if ([valueObject isKindOfClass:[NSArray class]])
        return [NSString stringWithFormat:@"<array of size %lu>", static_cast<unsigned long>([(NSArray*)valueObject count])];

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
        
        // Skip screen-specific information.
        if ([attribute isEqualToString:@"_AXPrimaryScreenHeight"] || [attribute isEqualToString:@"AXRelativeFrame"])
            continue;

        // accessibilityAttributeValue: can throw an if an attribute is not returned.
        // For DumpRenderTree's purpose, we should ignore those exceptions
        BEGIN_AX_OBJC_EXCEPTIONS
        id valueObject = [accessibilityObject accessibilityAttributeValue:attribute];
        NSString* value = descriptionOfValue(valueObject, accessibilityObject);
        [attributesString appendFormat:@"%@: %@\n", attribute, value];
        END_AX_OBJC_EXCEPTIONS
    }
    
    return attributesString;
}

template<typename T> static JSObjectRef convertVectorToObjectArray(JSContextRef context, Vector<T> const& elements)
{
    auto array = JSObjectMakeArray(context, 0, nullptr, nullptr);
    unsigned size = elements.size();
    for (unsigned i = 0; i < size; ++i)
        JSObjectSetPropertyAtIndex(context, array, i, JSObjectMake(context, elements[i].getJSClass(), new T(elements[i])), nullptr);
    return array;
}

static JSRetainPtr<JSStringRef> concatenateAttributeAndValue(NSString *attribute, NSString *value)
{
    Vector<UniChar> buffer([attribute length]);
    [attribute getCharacters:buffer.data()];
    buffer.append(':');
    buffer.append(' ');

    Vector<UniChar> valueBuffer([value length]);
    [value getCharacters:valueBuffer.data()];
    buffer.appendVector(valueBuffer);

    return adopt(JSStringCreateWithCharacters(buffer.data(), buffer.size()));
}

static JSRetainPtr<JSStringRef> descriptionOfElements(Vector<AccessibilityUIElement>& elementVector)
{
    NSMutableString* allElementString = [NSMutableString string];
    size_t size = elementVector.size();
    for (size_t i = 0; i < size; ++i) {
        NSString* attributes = attributesOfElement(elementVector[i].platformUIElement());
        [allElementString appendFormat:@"%@\n------------\n", attributes];
    }
    
    return [allElementString createJSStringRef];
}

static NSDictionary *selectTextParameterizedAttributeForCriteria(JSContextRef context, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
{
    NSMutableDictionary *parameterizedAttribute = [NSMutableDictionary dictionary];
    
    if (ambiguityResolution)
        [parameterizedAttribute setObject:[NSString stringWithJSStringRef:ambiguityResolution] forKey:@"AXSelectTextAmbiguityResolution"];
    
    if (searchStrings) {
        NSMutableArray *searchStringsParameter = [NSMutableArray array];
        if (JSValueIsString(context, searchStrings)) {
            auto searchStringsString = adopt(JSValueToStringCopy(context, searchStrings, nullptr));
            if (searchStringsString)
                [searchStringsParameter addObject:[NSString stringWithJSStringRef:searchStringsString.get()]];
        }
        else if (JSValueIsObject(context, searchStrings)) {
            auto searchStringsArray = (JSObjectRef)searchStrings;
            auto searchStringsArrayLength = WTR::arrayLength(context, searchStringsArray);
            for (unsigned i = 0; i < searchStringsArrayLength; ++i) {
                auto searchStringsString = adopt(JSValueToStringCopy(context, JSObjectGetPropertyAtIndex(context, searchStringsArray, i, nullptr), nullptr));
                if (searchStringsString)
                    [searchStringsParameter addObject:[NSString stringWithJSStringRef:searchStringsString.get()]];
            }
        }
        [parameterizedAttribute setObject:searchStringsParameter forKey:@"AXSelectTextSearchStrings"];
    }
    
    if (replacementString) {
        [parameterizedAttribute setObject:@"AXSelectTextActivityFindAndReplace" forKey:@"AXSelectTextActivity"];
        [parameterizedAttribute setObject:[NSString stringWithJSStringRef:replacementString] forKey:@"AXSelectTextReplacementString"];
    } else
        [parameterizedAttribute setObject:@"AXSelectTextActivityFindAndSelect" forKey:@"AXSelectTextActivity"];
    
    if (activity)
        [parameterizedAttribute setObject:[NSString stringWithJSStringRef:activity] forKey:@"AXSelectTextActivity"];
    
    return parameterizedAttribute;
}

#if PLATFORM(MAC)
static NSDictionary *searchTextParameterizedAttributeForCriteria(JSContextRef context, JSValueRef searchStrings, JSStringRef startFrom, JSStringRef direction)
{
    NSMutableDictionary *parameterizedAttribute = [NSMutableDictionary dictionary];

    if (searchStrings) {
        NSMutableArray *searchStringsParameter = [NSMutableArray array];
        if (JSValueIsString(context, searchStrings)) {
            auto searchStringsString = adopt(JSValueToStringCopy(context, searchStrings, nullptr));
            if (searchStringsString)
                [searchStringsParameter addObject:[NSString stringWithJSStringRef:searchStringsString.get()]];
        } else if (JSValueIsObject(context, searchStrings)) {
            auto searchStringsArray = (JSObjectRef)searchStrings;
            auto searchStringsArrayLength = WTR::arrayLength(context, searchStringsArray);
            for (unsigned i = 0; i < searchStringsArrayLength; ++i) {
                auto searchStringsString = adopt(JSValueToStringCopy(context, JSObjectGetPropertyAtIndex(context, searchStringsArray, i, nullptr), nullptr));
                if (searchStringsString)
                    [searchStringsParameter addObject:[NSString stringWithJSStringRef:searchStringsString.get()]];
            }
        }
        [parameterizedAttribute setObject:searchStringsParameter forKey:@"AXSearchTextSearchStrings"];
    }

    if (startFrom)
        [parameterizedAttribute setObject:[NSString stringWithJSStringRef:startFrom] forKey:@"AXSearchTextStartFrom"];

    if (direction)
        [parameterizedAttribute setObject:[NSString stringWithJSStringRef:direction] forKey:@"AXSearchTextDirection"];

    return parameterizedAttribute;
}
#endif

void AccessibilityUIElement::getLinkedUIElements(Vector<AccessibilityUIElement>& elementVector)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* linkedElements = [m_element accessibilityAttributeValue:NSAccessibilityLinkedUIElementsAttribute];
    elementVector = makeVector<AccessibilityUIElement>(linkedElements);
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::getDocumentLinks(Vector<AccessibilityUIElement>& elementVector)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* linkElements = [m_element accessibilityAttributeValue:@"AXLinkUIElements"];
    elementVector = makeVector<AccessibilityUIElement>(linkElements);
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::getChildren(Vector<AccessibilityUIElement>& elementVector)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* children = [m_element accessibilityAttributeValue:NSAccessibilityChildrenAttribute];
    elementVector = makeVector<AccessibilityUIElement>(children);
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::getChildrenWithRange(Vector<AccessibilityUIElement>& elementVector, unsigned location, unsigned length)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* children = [m_element accessibilityArrayAttributeValues:NSAccessibilityChildrenAttribute index:location maxCount:length];
    elementVector = makeVector<AccessibilityUIElement>(children);
    END_AX_OBJC_EXCEPTIONS
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

unsigned AccessibilityUIElement::indexOfChild(AccessibilityUIElement* element)
{
    return [m_element accessibilityIndexOfChild:element->platformUIElement()];
}

AccessibilityUIElement AccessibilityUIElement::getChildAtIndex(unsigned index)
{
    Vector<AccessibilityUIElement> children;
    getChildrenWithRange(children, index, 1);

    if (children.size() == 1)
        return children[0];
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::linkedUIElementAtIndex(unsigned index)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* objects = [m_element accessibilityAttributeValue:NSAccessibilityLinkedUIElementsAttribute];
    if (index < [objects count])
        return [objects objectAtIndex:index];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::ariaOwnsElementAtIndex(unsigned index)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* objects = [m_element accessibilityAttributeValue:NSAccessibilityOwnsAttribute];
    if (index < [objects count])
        return [objects objectAtIndex:index];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::ariaFlowToElementAtIndex(unsigned index)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* objects = [m_element accessibilityAttributeValue:NSAccessibilityLinkedUIElementsAttribute];
    if (index < [objects count])
        return [objects objectAtIndex:index];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::ariaControlsElementAtIndex(unsigned index)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* ariaControls = [m_element accessibilityAttributeValue:@"AXARIAControls"];
    if (index < [ariaControls count])
        return [ariaControls objectAtIndex:index];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::disclosedRowAtIndex(unsigned index)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* rows = [m_element accessibilityAttributeValue:NSAccessibilityDisclosedRowsAttribute];
    if (index < [rows count])
        return [rows objectAtIndex:index];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::selectedChildAtIndex(unsigned index) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* array = [m_element accessibilityAttributeValue:NSAccessibilitySelectedChildrenAttribute];
    if (index < [array count])
        return [array objectAtIndex:index];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

unsigned AccessibilityUIElement::selectedChildrenCount() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [m_element accessibilityArrayAttributeCount:NSAccessibilitySelectedChildrenAttribute];
    END_AX_OBJC_EXCEPTIONS

    return 0;
}

AccessibilityUIElement AccessibilityUIElement::selectedRowAtIndex(unsigned index)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* rows = [m_element accessibilityAttributeValue:NSAccessibilitySelectedRowsAttribute];
    if (index < [rows count])
        return [rows objectAtIndex:index];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::rowAtIndex(unsigned index)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* rows = [m_element accessibilityAttributeValue:NSAccessibilityRowsAttribute];
    if (index < [rows count])
        return [rows objectAtIndex:index];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::titleUIElement()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id accessibilityObject = [m_element accessibilityAttributeValue:NSAccessibilityTitleUIElementAttribute];
    if (accessibilityObject)
        return AccessibilityUIElement(accessibilityObject);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::parentElement()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id accessibilityObject = [m_element accessibilityAttributeValue:NSAccessibilityParentAttribute];
    if (accessibilityObject)
        return AccessibilityUIElement(accessibilityObject);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::disclosedByRow()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id accessibilityObject = [m_element accessibilityAttributeValue:NSAccessibilityDisclosedByRowAttribute];
    if (accessibilityObject)
        return AccessibilityUIElement(accessibilityObject);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfLinkedUIElements()
{
    Vector<AccessibilityUIElement> linkedElements;
    getLinkedUIElements(linkedElements);
    return descriptionOfElements(linkedElements);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfDocumentLinks()
{
    Vector<AccessibilityUIElement> linkElements;
    getDocumentLinks(linkElements);
    return descriptionOfElements(linkElements);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfChildren()
{
    Vector<AccessibilityUIElement> children;
    getChildren(children);
    return descriptionOfElements(children);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::allAttributes()
{
    return [attributesOfElement(m_element.get()) createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(JSStringRef attribute)
{
    return stringAttributeValue([NSString stringWithJSStringRef:attribute]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(NSString *attribute) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:attribute];
    if ([value isKindOfClass:[NSString class]])
        return [value createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

void AccessibilityUIElement::rowHeaders(Vector<AccessibilityUIElement>& elements) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityRowHeaderUIElementsAttribute];
    if ([value isKindOfClass:[NSArray class]])
        elements = makeVector<AccessibilityUIElement>(value);
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::columnHeaders(Vector<AccessibilityUIElement>& elements) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityColumnHeaderUIElementsAttribute];
    if ([value isKindOfClass:[NSArray class]])
        elements = makeVector<AccessibilityUIElement>(value);
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::uiElementArrayAttributeValue(JSStringRef attribute, Vector<AccessibilityUIElement>& elements) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:[NSString stringWithJSStringRef:attribute]];
    if ([value isKindOfClass:[NSArray class]])
        elements = makeVector<AccessibilityUIElement>(value);
    END_AX_OBJC_EXCEPTIONS
}

AccessibilityUIElement AccessibilityUIElement::uiElementAttributeValue(JSStringRef attribute) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id uiElement = [m_element accessibilityAttributeValue:[NSString stringWithJSStringRef:attribute]];
    return AccessibilityUIElement(uiElement);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}


double AccessibilityUIElement::numberAttributeValue(JSStringRef attribute)
{
    return numberAttributeValue([NSString stringWithJSStringRef:attribute]);
}

double AccessibilityUIElement::numberAttributeValue(NSString *attribute) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:attribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [value doubleValue];
    END_AX_OBJC_EXCEPTIONS
    
    return 0;
}

bool AccessibilityUIElement::boolAttributeValue(NSString *attribute) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:attribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    END_AX_OBJC_EXCEPTIONS
    
    return false;
}

bool AccessibilityUIElement::boolAttributeValue(JSStringRef attribute)
{
    return boolAttributeValue([NSString stringWithJSStringRef:attribute]);
}

void AccessibilityUIElement::setBoolAttributeValue(JSStringRef attribute, bool value)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element _accessibilitySetValue:@(value) forAttribute:[NSString stringWithJSStringRef:attribute]];
    END_AX_OBJC_EXCEPTIONS
}

bool AccessibilityUIElement::isAttributeSettable(JSStringRef attribute)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [m_element accessibilityIsAttributeSettable:[NSString stringWithJSStringRef:attribute]];
    END_AX_OBJC_EXCEPTIONS
    
    return false;
}

bool AccessibilityUIElement::isAttributeSupported(JSStringRef attribute)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [[m_element accessibilityAttributeNames] containsObject:[NSString stringWithJSStringRef:attribute]];
    END_AX_OBJC_EXCEPTIONS
    
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::parameterizedAttributeNames()
{
    NSArray* supportedParameterizedAttributes = [m_element accessibilityParameterizedAttributeNames];
    
    NSMutableString* attributesString = [NSMutableString string];
    for (NSUInteger i = 0; i < [supportedParameterizedAttributes count]; ++i) {
        [attributesString appendFormat:@"%@\n", [supportedParameterizedAttributes objectAtIndex:i]];
    }
    
    return [attributesString createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::role()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSString *role = descriptionOfValue([m_element accessibilityAttributeValue:NSAccessibilityRoleAttribute], m_element.get());
    return concatenateAttributeAndValue(@"AXRole", role);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::subrole()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSString* role = descriptionOfValue([m_element accessibilityAttributeValue:NSAccessibilitySubroleAttribute], m_element.get());
    return concatenateAttributeAndValue(@"AXSubrole", role);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::roleDescription()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSString* role = descriptionOfValue([m_element accessibilityAttributeValue:NSAccessibilityRoleDescriptionAttribute], m_element.get());
    return concatenateAttributeAndValue(@"AXRoleDescription", role);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::computedRoleString()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSString *computedRoleString = descriptionOfValue([m_element accessibilityAttributeValue:@"AXARIARole"], m_element.get());
    return [computedRoleString createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::title()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSString* title = descriptionOfValue([m_element accessibilityAttributeValue:NSAccessibilityTitleAttribute], m_element.get());
    return concatenateAttributeAndValue(@"AXTitle", title);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::description()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id description = descriptionOfValue([m_element accessibilityAttributeValue:NSAccessibilityDescriptionAttribute], m_element.get());
    return concatenateAttributeAndValue(@"AXDescription", description);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::domIdentifier() const
{
    return stringAttributeValue(NSAccessibilityDOMIdentifierAttribute);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::liveRegionRelevant() const
{
    return stringAttributeValue(@"AXARIARelevant");
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::liveRegionStatus() const
{
    return stringAttributeValue(@"AXARIALive");
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::orientation() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id description = descriptionOfValue([m_element accessibilityAttributeValue:NSAccessibilityOrientationAttribute], m_element.get());
    return concatenateAttributeAndValue(@"AXOrientation", description);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringValue()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id description = descriptionOfValue([m_element accessibilityAttributeValue:NSAccessibilityValueAttribute], m_element.get());
    if (description)
        return concatenateAttributeAndValue(@"AXValue", description);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::language()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id description = descriptionOfValue([m_element accessibilityAttributeValue:@"AXLanguage"], m_element.get());
    return concatenateAttributeAndValue(@"AXLanguage", description);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::helpText() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id description = descriptionOfValue([m_element accessibilityAttributeValue:NSAccessibilityHelpAttribute], m_element.get());
    return concatenateAttributeAndValue(@"AXHelp", description);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

double AccessibilityUIElement::x()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSValue* positionValue = [m_element accessibilityAttributeValue:NSAccessibilityPositionAttribute];
    return static_cast<double>([positionValue pointValue].x);    
    END_AX_OBJC_EXCEPTIONS
    
    return 0.0f;
}

double AccessibilityUIElement::y()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSValue* positionValue = [m_element accessibilityAttributeValue:NSAccessibilityPositionAttribute];
    return static_cast<double>([positionValue pointValue].y);    
    END_AX_OBJC_EXCEPTIONS
    
    return 0.0f;
}

double AccessibilityUIElement::width()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSValue* sizeValue = [m_element accessibilityAttributeValue:NSAccessibilitySizeAttribute];
    return static_cast<double>([sizeValue sizeValue].width);
    END_AX_OBJC_EXCEPTIONS
    
    return 0.0f;
}

double AccessibilityUIElement::height()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSValue* sizeValue = [m_element accessibilityAttributeValue:NSAccessibilitySizeAttribute];
    return static_cast<double>([sizeValue sizeValue].height);
    END_AX_OBJC_EXCEPTIONS
    
    return 0.0f;
}

double AccessibilityUIElement::clickPointX()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSValue* positionValue = [m_element accessibilityAttributeValue:@"AXClickPoint"];
    return static_cast<double>([positionValue pointValue].x);        
    END_AX_OBJC_EXCEPTIONS
    
    return 0.0f;
}

double AccessibilityUIElement::clickPointY()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSValue* positionValue = [m_element accessibilityAttributeValue:@"AXClickPoint"];
    return static_cast<double>([positionValue pointValue].y);
    END_AX_OBJC_EXCEPTIONS
    
    return 0.0f;
}

double AccessibilityUIElement::intValue() const
{
    return numberAttributeValue(NSAccessibilityValueAttribute);
}

double AccessibilityUIElement::minValue()
{
    return numberAttributeValue(NSAccessibilityMinValueAttribute);
}

double AccessibilityUIElement::maxValue()
{
    return numberAttributeValue(NSAccessibilityMaxValueAttribute);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::valueDescription()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSString* valueDescription = [m_element accessibilityAttributeValue:NSAccessibilityValueDescriptionAttribute];
    if ([valueDescription isKindOfClass:[NSString class]])
        return concatenateAttributeAndValue(@"AXValueDescription", valueDescription);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityInsertionPointLineNumberAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber *)value intValue]; 
    END_AX_OBJC_EXCEPTIONS
    
    return -1;
}

bool AccessibilityUIElement::isPressActionSupported()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* actions = [m_element accessibilityActionNames];
    return [actions containsObject:NSAccessibilityPressAction];
    END_AX_OBJC_EXCEPTIONS
    
    return false;
}

bool AccessibilityUIElement::isIncrementActionSupported()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* actions = [m_element accessibilityActionNames];
    return [actions containsObject:NSAccessibilityIncrementAction];
    END_AX_OBJC_EXCEPTIONS
    
    return false;
}

bool AccessibilityUIElement::isDecrementActionSupported()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* actions = [m_element accessibilityActionNames];
    return [actions containsObject:NSAccessibilityDecrementAction];
    END_AX_OBJC_EXCEPTIONS
    
    return false;
}

bool AccessibilityUIElement::isAtomicLiveRegion() const
{
    return boolAttributeValue(@"AXARIAAtomic");
}

bool AccessibilityUIElement::isBusy() const
{
    return boolAttributeValue(@"AXElementBusy");
}

bool AccessibilityUIElement::isEnabled()
{
    return boolAttributeValue(NSAccessibilityEnabledAttribute);
}

bool AccessibilityUIElement::isRequired() const
{
    return boolAttributeValue(@"AXRequired");
}

bool AccessibilityUIElement::isFocused() const
{
    return boolAttributeValue(NSAccessibilityFocusedAttribute);
}

bool AccessibilityUIElement::isSelected() const
{
    return boolAttributeValue(NSAccessibilitySelectedAttribute);
}

bool AccessibilityUIElement::isExpanded() const
{
    return boolAttributeValue(NSAccessibilityExpandedAttribute);
}

bool AccessibilityUIElement::isChecked() const
{
    // On the Mac, intValue()==1 if a a checkable control is checked.
    return intValue() == 1;
}

int AccessibilityUIElement::hierarchicalLevel() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityDisclosureLevelAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [value intValue];
    END_AX_OBJC_EXCEPTIONS

    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::speakAs()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:@"AXDRTSpeechAttribute"];
    if ([value isKindOfClass:[NSArray class]])
        return [[value componentsJoinedByString:@", "] createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
        
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::classList() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:@"AXDOMClassList"];
    if (![value isKindOfClass:[NSArray class]])
        return nullptr;
    
    NSMutableString* classList = [NSMutableString string];
    NSInteger length = [value count];
    for (NSInteger k = 0; k < length; ++k) {
        [classList appendString:[value objectAtIndex:k]];
        if (k < length - 1)
            [classList appendString:@", "];
    }
    
    return [classList createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

bool AccessibilityUIElement::ariaIsGrabbed() const
{
    return boolAttributeValue(NSAccessibilityGrabbedAttribute);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::embeddedImageDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSString *value = descriptionOfValue([m_element accessibilityAttributeValue:@"AXEmbeddedImageDescription"], m_element.get());
    return concatenateAttributeAndValue(@"AXEmbeddedImageDescription", value);
    END_AX_OBJC_EXCEPTIONS
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::ariaDropEffects() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityDropEffectsAttribute];
    if (![value isKindOfClass:[NSArray class]])
        return 0;

    NSMutableString* dropEffects = [NSMutableString string];
    NSInteger length = [value count];
    for (NSInteger k = 0; k < length; ++k) {
        [dropEffects appendString:[value objectAtIndex:k]];
        if (k < length - 1)
            [dropEffects appendString:@","];
    }
    
    return [dropEffects createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

// parameterized attributes
int AccessibilityUIElement::lineForIndex(int index)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityLineForIndexParameterizedAttribute forParameter:@(index)];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber *)value intValue]; 
    END_AX_OBJC_EXCEPTIONS

    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForLine(int line)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityRangeForLineParameterizedAttribute forParameter:@(line)];
    if ([value isKindOfClass:[NSValue class]])
        return [NSStringFromRange([value rangeValue]) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForPosition(int x, int y)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityRangeForPositionParameterizedAttribute forParameter:[NSValue valueWithPoint:NSMakePoint(x, y)]];
    if ([value isKindOfClass:[NSValue class]])
        return [NSStringFromRange([value rangeValue]) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}


JSRetainPtr<JSStringRef> AccessibilityUIElement::boundsForRange(unsigned location, unsigned length)
{
    NSRange range = NSMakeRange(location, length);
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityBoundsForRangeParameterizedAttribute forParameter:[NSValue valueWithRange:range]];
    NSRect rect = NSMakeRect(0,0,0,0);
    if ([value isKindOfClass:[NSValue class]])
        rect = [value rectValue]; 
    
    // don't return position information because it is platform dependent
    NSMutableString* boundsDescription = [NSMutableString stringWithFormat:@"{{%f, %f}, {%f, %f}}",-1.0f,-1.0f,rect.size.width,rect.size.height];
    return [boundsDescription createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForRange(unsigned location, unsigned length)
{
    NSRange range = NSMakeRange(location, length);
    BEGIN_AX_OBJC_EXCEPTIONS
    id string = [m_element accessibilityAttributeValue:NSAccessibilityStringForRangeParameterizedAttribute forParameter:[NSValue valueWithRange:range]];
    if (![string isKindOfClass:[NSString class]])
        return 0;
    
    return [string createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForRange(unsigned location, unsigned length)
{
    NSRange range = NSMakeRange(location, length);
    BEGIN_AX_OBJC_EXCEPTIONS
    NSAttributedString* string = [m_element accessibilityAttributeValue:NSAccessibilityAttributedStringForRangeParameterizedAttribute forParameter:[NSValue valueWithRange:range]];
    if (![string isKindOfClass:[NSAttributedString class]])
        return 0;
    
    NSString* stringWithAttrs = [string description];
    return [stringWithAttrs createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

bool AccessibilityUIElement::attributedStringRangeIsMisspelled(unsigned location, unsigned length)
{
    NSRange range = NSMakeRange(location, length);
    BEGIN_AX_OBJC_EXCEPTIONS
    NSAttributedString* string = [m_element accessibilityAttributeValue:NSAccessibilityAttributedStringForRangeParameterizedAttribute forParameter:[NSValue valueWithRange:range]];
    if (![string isKindOfClass:[NSAttributedString class]])
        return false;

    NSDictionary* attrs = [string attributesAtIndex:0 effectiveRange:nil];
    BOOL misspelled = [[attrs objectForKey:NSAccessibilityMisspelledTextAttribute] boolValue];
#if PLATFORM(MAC)
    if (misspelled)
        misspelled = [[attrs objectForKey:NSAccessibilityMarkedMisspelledTextAttribute] boolValue];
#endif
    return misspelled;
    END_AX_OBJC_EXCEPTIONS
    
    return false;
}

unsigned AccessibilityUIElement::uiElementCountForSearchPredicate(JSContextRef context, AccessibilityUIElement *startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameterizedAttribute = searchPredicateParameterizedAttributeForSearchCriteria(context, startElement, isDirectionNext, UINT_MAX, searchKey, searchText, visibleOnly, immediateDescendantsOnly);
    id value = [m_element accessibilityAttributeValue:@"AXUIElementCountForSearchPredicate" forParameter:parameterizedAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [value unsignedIntValue];
    END_AX_OBJC_EXCEPTIONS
    
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement *startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameterizedAttribute = searchPredicateParameterizedAttributeForSearchCriteria(context, startElement, isDirectionNext, 1, searchKey, searchText, visibleOnly, immediateDescendantsOnly);
    id value = [m_element accessibilityAttributeValue:@"AXUIElementsForSearchPredicate" forParameter:parameterizedAttribute];
    if ([value isKindOfClass:[NSArray class]])
        return AccessibilityUIElement([value lastObject]);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectTextWithCriteria(JSContextRef context, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameterizedAttribute = selectTextParameterizedAttributeForCriteria(context, ambiguityResolution, searchStrings, replacementString, activity);
    id result = [m_element accessibilityAttributeValue:@"AXSelectTextWithCriteria" forParameter:parameterizedAttribute];
    if ([result isKindOfClass:[NSString class]])
        return [result createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

#if PLATFORM(MAC)
JSValueRef AccessibilityUIElement::searchTextWithCriteria(JSContextRef context, JSValueRef searchStrings, JSStringRef startFrom, JSStringRef direction)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameterizedAttribute = searchTextParameterizedAttributeForCriteria(context, searchStrings, startFrom, direction);
    id result = [m_element accessibilityAttributeValue:@"AXSearchTextWithCriteria" forParameter:parameterizedAttribute];
    if ([result isKindOfClass:[NSArray class]])
        return convertVectorToObjectArray(context, makeVector<AccessibilityTextMarkerRange>(result));
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}
#endif

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumnHeaders()
{
    // not yet defined in AppKit... odd
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* columnHeadersArray = [m_element accessibilityAttributeValue:@"AXColumnHeaderUIElements"];
    auto columnHeadersVector = makeVector<AccessibilityUIElement>(columnHeadersArray);
    return descriptionOfElements(columnHeadersVector);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRowHeaders()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* rowHeadersArray = [m_element accessibilityAttributeValue:@"AXRowHeaderUIElements"];
    auto rowHeadersVector = makeVector<AccessibilityUIElement>(rowHeadersArray);
    return descriptionOfElements(rowHeadersVector);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumns()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* columnsArray = [m_element accessibilityAttributeValue:NSAccessibilityColumnsAttribute];
    auto columnsVector = makeVector<AccessibilityUIElement>(columnsArray);
    return descriptionOfElements(columnsVector);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRows()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* rowsArray = [m_element accessibilityAttributeValue:NSAccessibilityRowsAttribute];
    auto rowsVector = makeVector<AccessibilityUIElement>(rowsArray);
    return descriptionOfElements(rowsVector);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfVisibleCells()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* cellsArray = [m_element accessibilityAttributeValue:@"AXVisibleCells"];
    auto cellsVector = makeVector<AccessibilityUIElement>(cellsArray);
    return descriptionOfElements(cellsVector);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfHeader()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id headerObject = [m_element accessibilityAttributeValue:NSAccessibilityHeaderAttribute];
    if (!headerObject)
        return [@"" createJSStringRef];
    
    Vector<AccessibilityUIElement> headerVector;
    headerVector.append(headerObject);
    return descriptionOfElements(headerVector);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

int AccessibilityUIElement::rowCount()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [m_element accessibilityArrayAttributeCount:NSAccessibilityRowsAttribute];
    END_AX_OBJC_EXCEPTIONS
    
    return 0;
}

int AccessibilityUIElement::columnCount()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [m_element accessibilityArrayAttributeCount:NSAccessibilityColumnsAttribute];
    END_AX_OBJC_EXCEPTIONS
    
    return 0;
}

int AccessibilityUIElement::indexInTable()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSNumber* indexNumber = [m_element accessibilityAttributeValue:NSAccessibilityIndexAttribute];
    if (indexNumber)
        return [indexNumber intValue];
    END_AX_OBJC_EXCEPTIONS

    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rowIndexRange()
{
    NSRange range = NSMakeRange(0, 0);
    BEGIN_AX_OBJC_EXCEPTIONS
    NSValue* indexRange = [m_element accessibilityAttributeValue:@"AXRowIndexRange"];
    if (indexRange)
        range = [indexRange rangeValue];
    NSMutableString* rangeDescription = [NSMutableString stringWithFormat:@"{%lu, %lu}", static_cast<unsigned long>(range.location), static_cast<unsigned long>(range.length)];
    return [rangeDescription createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::columnIndexRange()
{
    NSRange range = NSMakeRange(0, 0);
    BEGIN_AX_OBJC_EXCEPTIONS
    NSNumber* indexRange = [m_element accessibilityAttributeValue:@"AXColumnIndexRange"];
    if (indexRange)
        range = [indexRange rangeValue];
    NSMutableString* rangeDescription = [NSMutableString stringWithFormat:@"{%lu, %lu}",static_cast<unsigned long>(range.location), static_cast<unsigned long>(range.length)];
    return [rangeDescription createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::cellForColumnAndRow(unsigned col, unsigned row)
{
    NSArray *colRowArray = @[@(col), @(row)];
    BEGIN_AX_OBJC_EXCEPTIONS
    return [m_element accessibilityAttributeValue:@"AXCellForColumnAndRow" forParameter:colRowArray];
    END_AX_OBJC_EXCEPTIONS    

    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::horizontalScrollbar() const
{
    if (!m_element)
        return nullptr;
    
    BEGIN_AX_OBJC_EXCEPTIONS
    return AccessibilityUIElement([m_element accessibilityAttributeValue:NSAccessibilityHorizontalScrollBarAttribute]);
    END_AX_OBJC_EXCEPTIONS    
    
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::verticalScrollbar() const
{
    if (!m_element)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    return AccessibilityUIElement([m_element accessibilityAttributeValue:NSAccessibilityVerticalScrollBarAttribute]);
    END_AX_OBJC_EXCEPTIONS        

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::pathDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSMutableString *result = [NSMutableString stringWithString:@"\nStart Path\n"];
    NSBezierPath *bezierPath = [m_element accessibilityAttributeValue:NSAccessibilityPathAttribute];
    
    NSUInteger elementCount = [bezierPath elementCount];
    for (NSUInteger i = 0; i < elementCount; i++) {
        switch ([bezierPath elementAtIndex:i]) {
        case NSMoveToBezierPathElement:
            [result appendString:@"\tMove to point\n"];
            break;
            
        case NSLineToBezierPathElement:
            [result appendString:@"\tLine to\n"];
            break;
            
        case NSCurveToBezierPathElement:
            [result appendString:@"\tCurve to\n"];
            break;
            
        case NSClosePathBezierPathElement:
            [result appendString:@"\tClose\n"];
            break;
        }
    }
    
    return [result createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectedTextRange()
{
    NSRange range = NSMakeRange(NSNotFound, 0);
    BEGIN_AX_OBJC_EXCEPTIONS
    NSValue *indexRange = [m_element accessibilityAttributeValue:NSAccessibilitySelectedTextRangeAttribute];
    if (indexRange)
        range = [indexRange rangeValue];
    NSMutableString *rangeDescription = [NSMutableString stringWithFormat:@"{%lu, %lu}", static_cast<unsigned long>(range.location), static_cast<unsigned long>(range.length)];
    return [rangeDescription createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

void AccessibilityUIElement::setSelectedTextRange(unsigned location, unsigned length)
{
    NSRange textRange = NSMakeRange(location, length);
    NSValue *textRangeValue = [NSValue valueWithRange:textRange];
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilitySetValue:textRangeValue forAttribute:NSAccessibilitySelectedTextRangeAttribute];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::setValue(JSStringRef valueText)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilitySetValue:[NSString stringWithJSStringRef:valueText] forAttribute:NSAccessibilityValueAttribute];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::dismiss()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilityPerformAction:@"AXDismissAction"];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::increment()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilityPerformAction:@"AXSyncIncrementAction"];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::decrement()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilityPerformAction:@"AXSyncDecrementAction"];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::showMenu()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilityPerformAction:NSAccessibilityShowMenuAction];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::press()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilityPerformAction:NSAccessibilityPressAction];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::setSelectedChild(AccessibilityUIElement* element) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* array = @[element->platformUIElement()];
    [m_element accessibilitySetValue:array forAttribute:NSAccessibilitySelectedChildrenAttribute];
    END_AX_OBJC_EXCEPTIONS    
}

void AccessibilityUIElement::setSelectedChildAtIndex(unsigned index) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    AccessibilityUIElement element = const_cast<AccessibilityUIElement*>(this)->getChildAtIndex(index);
    if (!element.platformUIElement())
        return;
    NSArray *selectedChildren = [m_element accessibilityAttributeValue:NSAccessibilitySelectedChildrenAttribute];
    NSArray *array = @[element.platformUIElement()];
    if (selectedChildren)
        array = [selectedChildren arrayByAddingObjectsFromArray:array];
    [m_element _accessibilitySetValue:array forAttribute:NSAccessibilitySelectedChildrenAttribute];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::removeSelectionAtIndex(unsigned index) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    AccessibilityUIElement element = const_cast<AccessibilityUIElement*>(this)->getChildAtIndex(index);
    NSArray *selectedChildren = [m_element accessibilityAttributeValue:NSAccessibilitySelectedChildrenAttribute];
    if (!element.platformUIElement() || !selectedChildren)
        return;
    NSMutableArray *array = [NSMutableArray arrayWithArray:selectedChildren];
    [array removeObject:element.platformUIElement()];
    [m_element _accessibilitySetValue:array forAttribute:NSAccessibilitySelectedChildrenAttribute];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::clearSelectedChildren() const
{
    // FIXME: Implement this function.
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::accessibilityValue() const
{
    // FIXME: Implement this function.
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentEncoding()
{
    if (auto result = stringAttributeValue(@"AXDocumentEncoding"))
        return result;
    
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentURI()
{
    if (auto result = stringAttributeValue(@"AXDocumentURI"))
        return result;
    
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::url()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSURL *url = [m_element accessibilityAttributeValue:NSAccessibilityURLAttribute];
    return [[url absoluteString] createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

bool AccessibilityUIElement::addNotificationListener(JSObjectRef functionCallback)
{
    if (!functionCallback)
        return false;
 
    // Mac programmers should not be adding more than one notification listener per element.
    // Other platforms may be different.
    if (m_notificationHandler)
        return false;

    m_notificationHandler = adoptNS([[AccessibilityNotificationHandler alloc] init]);
    [m_notificationHandler setPlatformElement:platformUIElement()];
    [m_notificationHandler setCallback:functionCallback];
    [m_notificationHandler startObserving];

    return true;
}

void AccessibilityUIElement::removeNotificationListener()
{
    // Mac programmers should not be trying to remove a listener that's already removed.
    ASSERT(m_notificationHandler);

    [m_notificationHandler stopObserving];
    m_notificationHandler = nil;
}

bool AccessibilityUIElement::isFocusable() const
{
    bool result = false;
    BEGIN_AX_OBJC_EXCEPTIONS
    result = [m_element accessibilityIsAttributeSettable:NSAccessibilityFocusedAttribute];
    END_AX_OBJC_EXCEPTIONS
    
    return result;
}

bool AccessibilityUIElement::isSelectable() const
{
    bool result = false;
    BEGIN_AX_OBJC_EXCEPTIONS
    result = [m_element accessibilityIsAttributeSettable:NSAccessibilitySelectedAttribute];
    END_AX_OBJC_EXCEPTIONS
    return result;
}

bool AccessibilityUIElement::isMultiSelectable() const
{
    BOOL result = NO;
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:@"AXIsMultiSelectable"];
    if ([value isKindOfClass:[NSNumber class]])
        result = [value boolValue];
    END_AX_OBJC_EXCEPTIONS
    return result;
}

bool AccessibilityUIElement::isSelectedOptionActive() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isVisible() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isOnScreen() const
{
    return boolAttributeValue(@"AXIsOnScreen");
}

bool AccessibilityUIElement::isOffScreen() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isCollapsed() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isIndeterminate() const
{
    BOOL result = NO;
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityValueAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        result = [value intValue] == 2;
    END_AX_OBJC_EXCEPTIONS
    return result;
}

bool AccessibilityUIElement::isIgnored() const
{
    BOOL result = NO;
    BEGIN_AX_OBJC_EXCEPTIONS
    result = [m_element accessibilityIsIgnored];
    END_AX_OBJC_EXCEPTIONS
    return result;
}

bool AccessibilityUIElement::isSingleLine() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isMultiLine() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::hasPopup() const
{
    return boolAttributeValue(@"AXHasPopup");
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::popupValue() const
{
    if (auto result = stringAttributeValue(@"AXPopupValue"))
        return result;

    return [@"false" createJSStringRef];
}

bool AccessibilityUIElement::hasDocumentRoleAncestor() const
{
    return boolAttributeValue(@"AXHasDocumentRoleAncestor");
}

bool AccessibilityUIElement::hasWebApplicationAncestor() const
{
    return boolAttributeValue(@"AXHasWebApplicationAncestor");
}

bool AccessibilityUIElement::isInDescriptionListDetail() const
{
    return boolAttributeValue(@"AXIsInDescriptionListDetail");
}

bool AccessibilityUIElement::isInDescriptionListTerm() const
{
    return boolAttributeValue(@"AXIsInDescriptionListTerm");
}

bool AccessibilityUIElement::isInCell() const
{
    return boolAttributeValue(@"AXIsInCell");
}

void AccessibilityUIElement::takeFocus()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilitySetValue:@YES forAttribute:NSAccessibilityFocusedAttribute];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::takeSelection()
{
    // FIXME: implement
}

void AccessibilityUIElement::addSelection()
{
    // FIXME: implement
}

void AccessibilityUIElement::removeSelection()
{
    // FIXME: implement
}

#if SUPPORTS_AX_TEXTMARKERS && PLATFORM(MAC)

// Text markers
AccessibilityTextMarkerRange AccessibilityUIElement::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXLineTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::misspellingTextMarkerRange(AccessibilityTextMarkerRange* start, bool forward)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSMutableDictionary *parameters = [NSMutableDictionary dictionary];
    [parameters setObject:start->platformTextMarkerRange() forKey:@"AXStartTextMarkerRange"];
    [parameters setObject:[NSNumber numberWithBool:forward] forKey:@"AXSearchTextDirection"];
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXMisspellingTextMarkerRange" forParameter:parameters];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::textMarkerRangeForElement(AccessibilityUIElement* element)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXTextMarkerRangeForUIElement" forParameter:element->platformUIElement()];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::selectedTextMarkerRange()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:NSAccessibilitySelectedTextMarkerRangeAttribute];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

void AccessibilityUIElement::resetSelectedTextMarkerRange()
{
    id start = [m_element accessibilityAttributeValue:@"AXStartTextMarker"];
    if (!start)
        return;
    
    NSArray* textMarkers = @[start, start];
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXTextMarkerRangeForUnorderedTextMarkers" forParameter:textMarkers];
    if (!textMarkerRange)
        return;
    
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element _accessibilitySetValue:textMarkerRange forAttribute:NSAccessibilitySelectedTextMarkerRangeAttribute];
    END_AX_OBJC_EXCEPTIONS
}

bool AccessibilityUIElement::replaceTextInRange(JSStringRef string, int location, int length)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [m_element accessibilityReplaceRange:NSMakeRange(location, length) withText:[NSString stringWithJSStringRef:string]];
    END_AX_OBJC_EXCEPTIONS
    return false;
}

bool AccessibilityUIElement::insertText(JSStringRef text)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [m_element accessibilityInsertText:[NSString stringWithJSStringRef:text]];
    END_AX_OBJC_EXCEPTIONS
    return false;
}

int AccessibilityUIElement::textMarkerRangeLength(AccessibilityTextMarkerRange* range)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSNumber* lengthValue = [m_element accessibilityAttributeValue:@"AXLengthForTextMarkerRange" forParameter:range->platformTextMarkerRange()];
    return [lengthValue intValue];
    END_AX_OBJC_EXCEPTIONS
    
    return 0;
}

bool AccessibilityUIElement::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef attribute, AccessibilityTextMarkerRange* range)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSAttributedString* string = [m_element accessibilityAttributeValue:@"AXAttributedStringForTextMarkerRange" forParameter:range->platformTextMarkerRange()];
    if (![string isKindOfClass:[NSAttributedString class]])
        return false;
    
    NSDictionary* attrs = [string attributesAtIndex:0 effectiveRange:nil];
    if ([attrs objectForKey:[NSString stringWithJSStringRef:attribute]])
        return true;    
    END_AX_OBJC_EXCEPTIONS
    
    return false;
}

int AccessibilityUIElement::indexForTextMarker(AccessibilityTextMarker* marker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSNumber* indexNumber = [m_element accessibilityAttributeValue:@"AXIndexForTextMarker" forParameter:marker->platformTextMarker()];
    return [indexNumber intValue];
    END_AX_OBJC_EXCEPTIONS
    
    return -1;
}

AccessibilityTextMarker AccessibilityUIElement::textMarkerForIndex(int textIndex)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXTextMarkerForIndex" forParameter:[NSNumber numberWithInteger:textIndex]];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

bool AccessibilityUIElement::isTextMarkerValid(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSNumber* validNumber = [m_element accessibilityAttributeValue:@"AXTextMarkerIsValid" forParameter:textMarker->platformTextMarker()];
    return [validNumber boolValue];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

AccessibilityTextMarker AccessibilityUIElement::previousTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id previousMarker = [m_element accessibilityAttributeValue:@"AXPreviousTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(previousMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::nextTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id nextMarker = [m_element accessibilityAttributeValue:@"AXNextTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(nextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textString = [m_element accessibilityAttributeValue:@"AXStringForTextMarkerRange" forParameter:markerRange->platformTextMarkerRange()];
    return [textString createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

static JSRetainPtr<JSStringRef> createJSStringRef(id string)
{
    if (!string)
        return nullptr;
    auto mutableString = adoptNS([[NSMutableString alloc] init]);
    id attributes = [string attributesAtIndex:0 effectiveRange:nil];
    id attributeEnumerationBlock = ^(NSDictionary<NSString *, id> *attrs, NSRange range, BOOL *stop) {
        BOOL misspelled = [[attrs objectForKey:NSAccessibilityMisspelledTextAttribute] boolValue];
        if (misspelled)
            misspelled = [[attrs objectForKey:NSAccessibilityMarkedMisspelledTextAttribute] boolValue];
        if (misspelled)
            [mutableString appendString:@"Misspelled, "];
        id font = [attributes objectForKey:(__bridge NSString *)kAXFontTextAttribute];
        if (font)
            [mutableString appendFormat:@"%@ - %@, ", (__bridge NSString *)kAXFontTextAttribute, font];
    };
    [string enumerateAttributesInRange:NSMakeRange(0, [string length]) options:(NSAttributedStringEnumerationOptions)0 usingBlock:attributeEnumerationBlock];
    [mutableString appendString:[string string]];
    return [mutableString createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    id string = [m_element accessibilityAttributeValue:@"AXAttributedStringForTextMarkerRange" forParameter:markerRange->platformTextMarkerRange()];
    return createJSStringRef(string);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange* markerRange, bool includeSpellCheck)
{
    id parameter = nil;
    if (includeSpellCheck)
        parameter = @{ @"AXSpellCheck": includeSpellCheck ? @YES : @NO, @"AXTextMarkerRange": markerRange->platformTextMarkerRange() };
    else
        parameter = markerRange->platformTextMarkerRange();
    id string = [m_element accessibilityAttributeValue:@"AXAttributedStringForTextMarkerRangeWithOptions" forParameter:parameter];
    return createJSStringRef(string);
}

AccessibilityTextMarkerRange AccessibilityUIElement::textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray *textMarkers = @[startMarker->platformTextMarker(), endMarker->platformTextMarker()];
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXTextMarkerRangeForTextMarkers" forParameter:textMarkers];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXStartTextMarkerForTextMarkerRange" forParameter:range->platformTextMarkerRange()];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXEndTextMarkerForTextMarkerRange" forParameter:range->platformTextMarkerRange()];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::endTextMarkerForBounds(int x, int y, int width, int height)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute forParameter:[NSValue valueWithRect:NSMakeRect(x, y, width, height)]];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::startTextMarkerForBounds(int x, int y, int width, int height)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute forParameter:[NSValue valueWithRect:NSMakeRect(x, y, width, height)]];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::textMarkerForPoint(int x, int y)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXTextMarkerForPosition" forParameter:[NSValue valueWithPoint:NSMakePoint(x, y)]];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityUIElement AccessibilityUIElement::accessibilityElementForTextMarker(AccessibilityTextMarker* marker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id uiElement = [m_element accessibilityAttributeValue:@"AXUIElementForTextMarker" forParameter:marker->platformTextMarker()];
    return AccessibilityUIElement(uiElement);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::startTextMarker()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXStartTextMarker"];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::endTextMarker()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXEndTextMarker"];
    return AccessibilityTextMarker(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

bool AccessibilityUIElement::setSelectedTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilitySetValue:markerRange->platformTextMarkerRange() forAttribute:NSAccessibilitySelectedTextMarkerRangeAttribute];
    END_AX_OBJC_EXCEPTIONS
    
    return true;
}

AccessibilityTextMarkerRange AccessibilityUIElement::leftWordTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXLeftWordTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::rightWordTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXRightWordTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::previousWordStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id previousTextMarker = [m_element accessibilityAttributeValue:@"AXPreviousWordStartTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(previousTextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::nextWordEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id nextTextMarker = [m_element accessibilityAttributeValue:@"AXNextWordEndTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(nextTextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::paragraphTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXParagraphTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::previousParagraphStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id previousTextMarker = [m_element accessibilityAttributeValue:@"AXPreviousParagraphStartTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(previousTextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::nextParagraphEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id nextTextMarker = [m_element accessibilityAttributeValue:@"AXNextParagraphEndTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(nextTextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::sentenceTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXSentenceTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::previousSentenceStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id previousTextMarker = [m_element accessibilityAttributeValue:@"AXPreviousSentenceStartTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(previousTextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::nextSentenceEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id nextTextMarker = [m_element accessibilityAttributeValue:@"AXNextSentenceEndTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(nextTextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

#endif // SUPPORTS_AX_TEXTMARKERS && PLATFORM(MAC)

JSRetainPtr<JSStringRef> AccessibilityUIElement::supportedActions()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray *names = [m_element accessibilityActionNames];
    return [[names componentsJoinedByString:@","] createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

static NSString *convertMathMultiscriptPairsToString(NSArray *pairs)
{
    __block NSMutableString *result = [NSMutableString string];
    [pairs enumerateObjectsUsingBlock:^(id pair, NSUInteger index, BOOL *stop) {
        for (NSString *key in pair)
            [result appendFormat:@"\t%lu. %@ = %@\n", (unsigned long)index, key, [[pair objectForKey:key] accessibilityAttributeValue:NSAccessibilitySubroleAttribute]];
    }];
    
    return result;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPostscriptsDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray *pairs = [m_element accessibilityAttributeValue:@"AXMathPostscripts"];
    return [convertMathMultiscriptPairsToString(pairs) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPrescriptsDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray *pairs = [m_element accessibilityAttributeValue:@"AXMathPrescripts"];
    return [convertMathMultiscriptPairsToString(pairs) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

void AccessibilityUIElement::scrollToMakeVisible()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilityPerformAction:@"AXScrollToVisible"];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element _accessibilityScrollToMakeVisibleWithSubFocus:NSMakeRect(x, y, width, height)];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::scrollToGlobalPoint(int x, int y)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element _accessibilityScrollToGlobalPoint:NSMakePoint(x, y)];
    END_AX_OBJC_EXCEPTIONS
}
