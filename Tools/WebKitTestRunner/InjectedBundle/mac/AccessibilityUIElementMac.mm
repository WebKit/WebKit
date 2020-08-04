/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import "AccessibilityCommonMac.h"

#if HAVE(ACCESSIBILITY)

#import "AccessibilityNotificationHandler.h"
#import "AccessibilityUIElement.h"
#import "InjectedBundle.h"
#import "InjectedBundlePage.h"

#import <AppKit/NSAccessibility.h>
#import <Foundation/Foundation.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <JavaScriptCore/JSObjectRef.h>
#import <WebKit/WKBundleFrame.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/VectorCocoa.h>

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
- (BOOL)isIsolatedObject;
- (BOOL)accessibilityReplaceRange:(NSRange)range withText:(NSString *)string;
- (BOOL)accessibilityInsertText:(NSString *)text;
- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute index:(NSUInteger)index maxCount:(NSUInteger)maxCount;
- (NSUInteger)accessibilityIndexOfChild:(id)child;
- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute;
- (void)_accessibilityScrollToMakeVisibleWithSubFocus:(NSRect)rect;
- (void)_accessibilityScrollToGlobalPoint:(NSPoint)point;
- (void)_accessibilitySetValue:(id)value forAttribute:(NSString*)attributeName;
@end

namespace WTR {

static JSRetainPtr<JSStringRef> createEmptyJSString()
{
    return adopt(JSStringCreateWithCharacters(nullptr, 0));
}

RefPtr<AccessibilityController> AccessibilityUIElement::s_controller;

AccessibilityUIElement::AccessibilityUIElement(id element)
    : m_element(element)
{
    if (!s_controller)
        s_controller = InjectedBundle::singleton().accessibilityController();
}

AccessibilityUIElement::AccessibilityUIElement(const AccessibilityUIElement& other)
    : JSWrappable()
    , m_element(other.m_element)
{
}

AccessibilityUIElement::~AccessibilityUIElement()
{
}

bool AccessibilityUIElement::isEqual(AccessibilityUIElement* otherElement)
{
    if (!otherElement)
        return false;
    return platformUIElement() == otherElement->platformUIElement();
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
bool AccessibilityUIElement::isIsolatedObject() const
{
    return [m_element isIsolatedObject];
}
#endif

RetainPtr<NSArray> supportedAttributes(id element)
{
    RetainPtr<NSArray> attributes;

    BEGIN_AX_OBJC_EXCEPTIONS
    AccessibilityUIElement::s_controller->executeOnAXThreadAndWait([&attributes, &element] {
        attributes = [element accessibilityAttributeNames];
    });
    END_AX_OBJC_EXCEPTIONS

    return attributes;
}

id attributeValue(id element, NSString *attribute)
{
    RetainPtr<id> value;

    BEGIN_AX_OBJC_EXCEPTIONS
    AccessibilityUIElement::s_controller->executeOnAXThreadAndWait([&element, &attribute, &value] {
        value = [element accessibilityAttributeValue:attribute];
    });
    END_AX_OBJC_EXCEPTIONS

    return value.get();
}

void setAttributeValue(id element, NSString* attribute, id value, bool synchronous = false)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    AccessibilityUIElement::s_controller->executeOnAXThreadAndWait([&element, &attribute, &value, &synchronous] {
        // FIXME: should always be asynchronous, fix tests.
        synchronous ? [element _accessibilitySetValue:value forAttribute:attribute] :
            [element accessibilitySetValue:value forAttribute:attribute];
    });
    END_AX_OBJC_EXCEPTIONS
}

static NSString* descriptionOfValue(id valueObject, id element)
{
    if (!valueObject)
        return nil;

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
        NSString* role = attributeValue(element, NSAccessibilityRoleAttribute);
        NSString* title = attributeValue(element, NSAccessibilityTitleAttribute);
        if (title.length)
            return [NSString stringWithFormat:@"<%@: '%@'>", role, title];
        return [NSString stringWithFormat:@"<%@>", role];
    }

    return [valueObject description];
}

static NSString *attributesOfElement(id accessibilityObject)
{
    RetainPtr<NSArray> attributes = supportedAttributes(accessibilityObject);

    NSMutableString* attributesString = [NSMutableString string];
    for (NSString* attribute in attributes.get()) {
        // Position provides useless and screen-specific information, so we do not
        // want to include it for the sake of universally passing tests.
        if ([attribute isEqualToString:@"AXPosition"])
            continue;

        // Skip screen-specific information.
        if ([attribute isEqualToString:@"_AXPrimaryScreenHeight"] || [attribute isEqualToString:@"AXRelativeFrame"])
            continue;

        id valueObject = attributeValue(accessibilityObject, attribute);
        NSString* value = descriptionOfValue(valueObject, accessibilityObject);
        [attributesString appendFormat:@"%@: %@\n", attribute, value];
    }

    return attributesString;
}

template<typename T>
static JSValueRef convertVectorToObjectArray(JSContextRef context, Vector<T> const& elements)
{
    JSValueRef arrayResult = JSObjectMakeArray(context, 0, 0, 0);
    JSObjectRef arrayObj = JSValueToObject(context, arrayResult, 0);
    size_t elementCount = elements.size();
    for (size_t i = 0; i < elementCount; ++i)
        JSObjectSetPropertyAtIndex(context, arrayObj, i, JSObjectMake(context, elements[i]->wrapperClass(), elements[i].get()), 0);

    return arrayResult;
}

static JSRetainPtr<JSStringRef> concatenateAttributeAndValue(NSString* attribute, NSString* value)
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

static JSRetainPtr<JSStringRef> descriptionOfElements(Vector<RefPtr<AccessibilityUIElement> >& elementVector)
{
    NSMutableString* allElementString = [NSMutableString string];
    for (auto uiElement : elementVector) {
        NSString* attributes = attributesOfElement(uiElement->platformUIElement());
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
            JSObjectRef searchStringsArray = JSValueToObject(context, searchStrings, nullptr);
            unsigned searchStringsArrayLength = 0;
            
            auto lengthPropertyString = adopt(JSStringCreateWithUTF8CString("length"));
            JSValueRef searchStringsArrayLengthValue = JSObjectGetProperty(context, searchStringsArray, lengthPropertyString.get(), nullptr);
            if (searchStringsArrayLengthValue && JSValueIsNumber(context, searchStringsArrayLengthValue))
                searchStringsArrayLength = static_cast<unsigned>(JSValueToNumber(context, searchStringsArrayLengthValue, nullptr));
            
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
            JSObjectRef searchStringsArray = JSValueToObject(context, searchStrings, nullptr);
            unsigned searchStringsArrayLength = 0;

            auto lengthPropertyString = adopt(JSStringCreateWithUTF8CString("length"));
            JSValueRef searchStringsArrayLengthValue = JSObjectGetProperty(context, searchStringsArray, lengthPropertyString.get(), nullptr);
            if (searchStringsArrayLengthValue && JSValueIsNumber(context, searchStringsArrayLengthValue))
                searchStringsArrayLength = static_cast<unsigned>(JSValueToNumber(context, searchStringsArrayLengthValue, nullptr));

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

static NSDictionary *misspellingSearchParameterizedAttributeForCriteria(AccessibilityTextMarkerRange* start, bool forward)
{
    if (!start || !start->platformTextMarkerRange())
        return nil;

    NSMutableDictionary *parameters = [NSMutableDictionary dictionary];

    [parameters setObject:start->platformTextMarkerRange() forKey:@"AXStartTextMarkerRange"];
    [parameters setObject:[NSNumber numberWithBool:forward] forKey:@"AXSearchTextDirection"];

    return parameters;
}

void AccessibilityUIElement::getLinkedUIElements(Vector<RefPtr<AccessibilityUIElement>>& elementVector)
{
    elementVector = makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(m_element.get(), NSAccessibilityLinkedUIElementsAttribute));
}

void AccessibilityUIElement::getDocumentLinks(Vector<RefPtr<AccessibilityUIElement>>& elementVector)
{
    elementVector = makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(m_element.get(), @"AXLinkUIElements"));
}

void AccessibilityUIElement::getUIElementsWithAttribute(JSStringRef attribute, Vector<RefPtr<AccessibilityUIElement>>& elements) const
{
    id value = attributeValue(m_element.get(), [NSString stringWithJSStringRef:attribute]);
    if ([value isKindOfClass:[NSArray class]])
        elements = makeVector<RefPtr<AccessibilityUIElement>>(value);
}

void AccessibilityUIElement::getChildren(Vector<RefPtr<AccessibilityUIElement> >& elementVector)
{
    elementVector = makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(m_element.get(), NSAccessibilityChildrenAttribute));
}

void AccessibilityUIElement::getChildrenWithRange(Vector<RefPtr<AccessibilityUIElement> >& elementVector, unsigned location, unsigned length)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    RetainPtr<NSArray> children;
    s_controller->executeOnAXThreadAndWait([&children, location, length, this] {
        children = [m_element accessibilityArrayAttributeValues:NSAccessibilityChildrenAttribute index:location maxCount:length];
    });
    elementVector = makeVector<RefPtr<AccessibilityUIElement>>(children.get());
    END_AX_OBJC_EXCEPTIONS
}

JSValueRef AccessibilityUIElement::rowHeaders() const
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);
    
    BEGIN_AX_OBJC_EXCEPTIONS
    Vector<RefPtr<AccessibilityUIElement>> elements;
    id value = attributeValue(m_element.get(), NSAccessibilityRowHeaderUIElementsAttribute);
    if ([value isKindOfClass:[NSArray class]])
        elements = makeVector<RefPtr<AccessibilityUIElement>>(value);
    return convertVectorToObjectArray<RefPtr<AccessibilityUIElement>>(context, elements);
    END_AX_OBJC_EXCEPTIONS
}

JSValueRef AccessibilityUIElement::columnHeaders() const
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);

    BEGIN_AX_OBJC_EXCEPTIONS
    Vector<RefPtr<AccessibilityUIElement>> elements;
    id value = attributeValue(m_element.get(), NSAccessibilityColumnHeaderUIElementsAttribute);
    if ([value isKindOfClass:[NSArray class]])
        elements = makeVector<RefPtr<AccessibilityUIElement>>(value);
    return convertVectorToObjectArray<RefPtr<AccessibilityUIElement>>(context, elements);
    END_AX_OBJC_EXCEPTIONS
}
    
int AccessibilityUIElement::childrenCount()
{
    Vector<RefPtr<AccessibilityUIElement> > children;
    getChildren(children);
    
    return children.size();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::elementAtPoint(int x, int y)
{
    RetainPtr<id> element;
    s_controller->executeOnAXThreadAndWait([&x, &y, &element, this] {
        element = [m_element accessibilityHitTest:NSMakePoint(x, y)];
    });

    if (!element)
        return nullptr;

    return AccessibilityUIElement::create(element.get());
}

unsigned AccessibilityUIElement::indexOfChild(AccessibilityUIElement* element)
{
    unsigned index;
    id platformElement = element->platformUIElement();
    s_controller->executeOnAXThreadAndWait([&platformElement, &index, this] {
        index = [m_element accessibilityIndexOfChild:platformElement];
    });
    return index;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::childAtIndex(unsigned index)
{
    Vector<RefPtr<AccessibilityUIElement>> children;
    getChildrenWithRange(children, index, 1);

    if (children.size() == 1)
        return children[0];
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::elementForAttribute(NSString* attribute) const
{
    id element = attributeValue(m_element.get(), attribute);
    return element ? AccessibilityUIElement::create(element) : RefPtr<AccessibilityUIElement>();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::elementForAttributeAtIndex(NSString* attribute, unsigned index) const
{
    NSArray* elements = attributeValue(m_element.get(), attribute);
    return index < elements.count ? AccessibilityUIElement::create([elements objectAtIndex:index]) : RefPtr<AccessibilityUIElement>();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::linkedUIElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(NSAccessibilityLinkedUIElementsAttribute, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaOwnsElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(NSAccessibilityOwnsAttribute, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaFlowToElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(NSAccessibilityLinkedUIElementsAttribute, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaControlsElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXARIAControls", index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaDetailsElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXDetailsElements", index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::ariaErrorMessageElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXErrorMessageElements", index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedRowAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(NSAccessibilityDisclosedRowsAttribute, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::rowAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(NSAccessibilityRowsAttribute, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedChildAtIndex(unsigned index) const
{
    return elementForAttributeAtIndex(NSAccessibilitySelectedChildrenAttribute, index);
}

unsigned AccessibilityUIElement::selectedChildrenCount() const
{
    unsigned count = 0;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([&count, this] {
        count = [m_element accessibilityArrayAttributeCount:NSAccessibilitySelectedChildrenAttribute];
    });
    END_AX_OBJC_EXCEPTIONS

    return count;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::selectedRowAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(NSAccessibilitySelectedRowsAttribute, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::titleUIElement()
{
    return elementForAttribute(NSAccessibilityTitleUIElementAttribute);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::parentElement()
{
    return elementForAttribute(NSAccessibilityParentAttribute);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::disclosedByRow()
{
    return elementForAttribute(NSAccessibilityDisclosedByRowAttribute);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfLinkedUIElements()
{
    Vector<RefPtr<AccessibilityUIElement> > linkedElements;
    getLinkedUIElements(linkedElements);
    return descriptionOfElements(linkedElements);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfDocumentLinks()
{
    Vector<RefPtr<AccessibilityUIElement> > linkElements;
    getDocumentLinks(linkElements);
    return descriptionOfElements(linkElements);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfChildren()
{
    Vector<RefPtr<AccessibilityUIElement> > children;
    getChildren(children);
    return descriptionOfElements(children);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::allAttributes()
{
    NSString* attributes = attributesOfElement(m_element.get());
    return [attributes createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringDescriptionOfAttributeValue(JSStringRef attribute)
{
    id value = attributeValue(m_element.get(), [NSString stringWithJSStringRef:attribute]);
    NSString* valueDescription = descriptionOfValue(value, m_element.get());
    return [valueDescription createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(JSStringRef attribute)
{
    id value = attributeValue(m_element.get(), [NSString stringWithJSStringRef:attribute]);
    if ([value isKindOfClass:[NSString class]])
        return [value createJSStringRef];
    return nullptr;
}

double AccessibilityUIElement::numberAttributeValue(JSStringRef attribute)
{
    id value = attributeValue(m_element.get(), [NSString stringWithJSStringRef:attribute]);
    if ([value isKindOfClass:[NSNumber class]])
        return [value doubleValue];
    return 0;
}

JSValueRef AccessibilityUIElement::uiElementArrayAttributeValue(JSStringRef attribute) const
{
    WKBundleFrameRef mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
    JSContextRef context = WKBundleFrameGetJavaScriptContext(mainFrame);

    Vector<RefPtr<AccessibilityUIElement>> elements;
    getUIElementsWithAttribute(attribute, elements);
    return convertVectorToObjectArray<RefPtr<AccessibilityUIElement>>(context, elements);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementAttributeValue(JSStringRef attribute) const
{
    id value = attributeValue(m_element.get(), [NSString stringWithJSStringRef:attribute]);
    return AccessibilityUIElement::create(value);
}

bool AccessibilityUIElement::boolAttributeValue(JSStringRef attribute)
{
    id value = attributeValue(m_element.get(), [NSString stringWithJSStringRef:attribute]);
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    return false;
}

void AccessibilityUIElement::attributeValueAsync(JSStringRef attribute, JSValueRef callback)
{
    if (!attribute || !callback)
        return;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThread([attribute = retainPtr([NSString stringWithJSStringRef:attribute]), callback = WTFMove(callback), this] () mutable {
        id value = [m_element accessibilityAttributeValue:attribute.get()];

        s_controller->executeOnMainThread([value = retainPtr(value), callback = WTFMove(callback)] () {
            auto mainFrame = WKBundlePageGetMainFrame(InjectedBundle::singleton().page()->page());
            auto context = WKBundleFrameGetJavaScriptContext(mainFrame);

            JSValueRef arguments[1];
            arguments[0] = makeValueRefForValue(context, value.get());
            JSObjectCallAsFunction(context, const_cast<JSObjectRef>(callback), 0, 1, arguments, 0);
        });
    });
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::setBoolAttributeValue(JSStringRef attribute, bool value)
{
    setAttributeValue(m_element.get(), [NSString stringWithJSStringRef:attribute], @(value), true);
}

void AccessibilityUIElement::setValue(JSStringRef value)
{
    setAttributeValue(m_element.get(), NSAccessibilityValueAttribute, [NSString stringWithJSStringRef:value]);
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
    return [supportedAttributes(m_element.get()) containsObject:[NSString stringWithJSStringRef:attribute]];
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
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityValueAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber*)value doubleValue]; 
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElement::minValue()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityMinValueAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber*)value doubleValue]; 
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElement::maxValue()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityMaxValueAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber*)value doubleValue]; 
    END_AX_OBJC_EXCEPTIONS

    return 0.0;
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

bool AccessibilityUIElement::isEnabled()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityEnabledAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    END_AX_OBJC_EXCEPTIONS
    
    return false;
}

bool AccessibilityUIElement::isRequired() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:@"AXRequired"];
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    END_AX_OBJC_EXCEPTIONS
    
    return false;
}

bool AccessibilityUIElement::isFocused() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityFocusedAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    END_AX_OBJC_EXCEPTIONS
    
    return false;
}

bool AccessibilityUIElement::isSelected() const
{
    id value = attributeValue(m_element.get(), NSAccessibilitySelectedAttribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    return false;
}

bool AccessibilityUIElement::isSelectedOptionActive() const
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

bool AccessibilityUIElement::isExpanded() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityExpandedAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    END_AX_OBJC_EXCEPTIONS
    
    return false;
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

JSRetainPtr<JSStringRef> AccessibilityUIElement::speakAs()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:@"AXDRTSpeechAttribute"];
    if ([value isKindOfClass:[NSArray class]])
        return [[(NSArray *)value componentsJoinedByString:@", "] createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
        
    return nullptr;
}

bool AccessibilityUIElement::ariaIsGrabbed() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityGrabbedAttribute];
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::ariaDropEffects() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:NSAccessibilityDropEffectsAttribute];
    if (![value isKindOfClass:[NSArray class]])
        return nullptr;

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
        return nullptr;
    
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
        return nullptr;
    
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

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement *startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameterizedAttribute = searchPredicateParameterizedAttributeForSearchCriteria(context, startElement, isDirectionNext, 1, searchKey, searchText, visibleOnly, immediateDescendantsOnly);
    id value = [m_element accessibilityAttributeValue:@"AXUIElementsForSearchPredicate" forParameter:parameterizedAttribute];
    if ([value isKindOfClass:[NSArray class]])
        return AccessibilityUIElement::create([value lastObject]);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectTextWithCriteria(JSContextRef context, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameterizedAttribute = selectTextParameterizedAttributeForCriteria(context, ambiguityResolution, searchStrings, replacementString, activity);
    RetainPtr<id> result;
    s_controller->executeOnAXThreadAndWait([&parameterizedAttribute, &result, this] {
        result = [m_element accessibilityAttributeValue:@"AXSelectTextWithCriteria" forParameter:parameterizedAttribute];
    });
    if ([result.get() isKindOfClass:[NSString class]])
        return [result.get() createJSStringRef];
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
        return convertVectorToObjectArray<RefPtr<AccessibilityTextMarkerRange>>(context, makeVector<RefPtr<AccessibilityTextMarkerRange>>(result));
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}
#endif

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumnHeaders()
{
    // not yet defined in AppKit... odd
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* columnHeadersArray = [m_element accessibilityAttributeValue:@"AXColumnHeaderUIElements"];
    auto columnHeadersVector = makeVector<RefPtr<AccessibilityUIElement>>(columnHeadersArray);
    return descriptionOfElements(columnHeadersVector);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRowHeaders()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* rowHeadersArray = [m_element accessibilityAttributeValue:@"AXRowHeaderUIElements"];
    auto rowHeadersVector = makeVector<RefPtr<AccessibilityUIElement>>(rowHeadersArray);
    return descriptionOfElements(rowHeadersVector);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumns()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* columnsArray = [m_element accessibilityAttributeValue:NSAccessibilityColumnsAttribute];
    auto columnsVector = makeVector<RefPtr<AccessibilityUIElement>>(columnsArray);
    return descriptionOfElements(columnsVector);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRows()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* rowsArray = [m_element accessibilityAttributeValue:NSAccessibilityRowsAttribute];
    auto rowsVector = makeVector<RefPtr<AccessibilityUIElement>>(rowsArray);
    return descriptionOfElements(rowsVector);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfVisibleCells()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray* cellsArray = [m_element accessibilityAttributeValue:@"AXVisibleCells"];
    auto cellsVector = makeVector<RefPtr<AccessibilityUIElement>>(cellsArray);
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
    
    Vector<RefPtr<AccessibilityUIElement> > headerVector;
    headerVector.append(AccessibilityUIElement::create(headerObject));
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
    NSMutableString* rangeDescription = [NSMutableString stringWithFormat:@"{%lu, %lu}", static_cast<unsigned long>(range.location), static_cast<unsigned long>(range.length)];
    return [rangeDescription createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::cellForColumnAndRow(unsigned col, unsigned row)
{
    NSArray *colRowArray = @[@(col), @(row)];
    BEGIN_AX_OBJC_EXCEPTIONS
    return AccessibilityUIElement::create([m_element accessibilityAttributeValue:@"AXCellForColumnAndRow" forParameter:colRowArray]);
    END_AX_OBJC_EXCEPTIONS    

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::horizontalScrollbar() const
{
    if (!m_element)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    return AccessibilityUIElement::create([m_element accessibilityAttributeValue:NSAccessibilityHorizontalScrollBarAttribute]);
    END_AX_OBJC_EXCEPTIONS    
    
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::verticalScrollbar() const
{
    if (!m_element)
        return nullptr;
    
    BEGIN_AX_OBJC_EXCEPTIONS
    return AccessibilityUIElement::create([m_element accessibilityAttributeValue:NSAccessibilityVerticalScrollBarAttribute]);
    END_AX_OBJC_EXCEPTIONS        

    return nullptr;
}

void AccessibilityUIElement::scrollToMakeVisible()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilityPerformAction:@"AXScrollToVisible"];
    END_AX_OBJC_EXCEPTIONS
}
    
void AccessibilityUIElement::scrollToGlobalPoint(int x, int y)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element _accessibilityScrollToGlobalPoint:NSMakePoint(x, y)];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element _accessibilityScrollToMakeVisibleWithSubFocus:NSMakeRect(x, y, width, height)];
    END_AX_OBJC_EXCEPTIONS
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectedTextRange()
{
    NSRange range = NSMakeRange(NSNotFound, 0);
    BEGIN_AX_OBJC_EXCEPTIONS
    NSValue *indexRange = attributeValue(m_element.get(), NSAccessibilitySelectedTextRangeAttribute);
    if (indexRange)
        range = [indexRange rangeValue];
    NSMutableString* rangeDescription = [NSMutableString stringWithFormat:@"{%lu, %lu}", static_cast<unsigned long>(range.location), static_cast<unsigned long>(range.length)];
    return [rangeDescription createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

bool AccessibilityUIElement::setSelectedTextRange(unsigned location, unsigned length)
{
    NSRange textRange = NSMakeRange(location, length);
    NSValue *textRangeValue = [NSValue valueWithRange:textRange];
    setAttributeValue(m_element.get(), NSAccessibilitySelectedTextRangeAttribute, textRangeValue);

    return true;
}

void AccessibilityUIElement::dismiss()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilityPerformAction:@"AXDismissAction"];
    END_AX_OBJC_EXCEPTIONS
}

bool AccessibilityUIElement::setSelectedVisibleTextRange(AccessibilityTextMarkerRange* markerRange)
{
    if (!markerRange)
        return false;

    setAttributeValue(m_element.get(), NSAccessibilitySelectedTextMarkerRangeAttribute, markerRange->platformTextMarkerRange());

    return true;
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

void AccessibilityUIElement::asyncIncrement()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilityPerformAction:NSAccessibilityIncrementAction];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::asyncDecrement()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilityPerformAction:NSAccessibilityDecrementAction];
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

void AccessibilityUIElement::syncPress()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    [m_element accessibilityPerformAction:@"AXSyncPressAction"];
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::setSelectedChild(AccessibilityUIElement* element) const
{
    NSArray* array = element ? @[element->platformUIElement()] : @[];
    setAttributeValue(m_element.get(), NSAccessibilitySelectedChildrenAttribute, array);
}

void AccessibilityUIElement::setSelectedChildAtIndex(unsigned index) const
{
    RefPtr<AccessibilityUIElement> element = const_cast<AccessibilityUIElement*>(this)->childAtIndex(index);
    if (!element)
        return;

    NSArray *selectedChildren = attributeValue(m_element.get(), NSAccessibilitySelectedChildrenAttribute);
    NSArray *array = @[element->platformUIElement()];
    if (selectedChildren)
        array = [selectedChildren arrayByAddingObjectsFromArray:array];

    setAttributeValue(m_element.get(), NSAccessibilitySelectedChildrenAttribute, array, true);
}

void AccessibilityUIElement::removeSelectionAtIndex(unsigned index) const
{
    RefPtr<AccessibilityUIElement> element = const_cast<AccessibilityUIElement*>(this)->childAtIndex(index);
    if (!element)
        return;

    NSArray *selectedChildren = attributeValue(m_element.get(), NSAccessibilitySelectedChildrenAttribute);
    if (!selectedChildren)
        return;

    NSMutableArray *array = [NSMutableArray arrayWithArray:selectedChildren];
    [array removeObject:element->platformUIElement()];

    setAttributeValue(m_element.get(), NSAccessibilitySelectedChildrenAttribute, array, true);
}

void AccessibilityUIElement::clearSelectedChildren() const
{
    // FIXME: implement
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::accessibilityValue() const
{
    // FIXME: implement
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentEncoding()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:@"AXDocumentEncoding"];
    if ([value isKindOfClass:[NSString class]])
        return [value createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentURI()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:@"AXDocumentURI"];
    if ([value isKindOfClass:[NSString class]])
        return [value createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::url()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSURL *url = [m_element accessibilityAttributeValue:NSAccessibilityURLAttribute];
    return [[url absoluteString] createJSStringRef];    
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

bool AccessibilityUIElement::addNotificationListener(JSValueRef functionCallback)
{
    if (!functionCallback)
        return false;
 
    // Mac programmers should not be adding more than one notification listener per element.
    // Other platforms may be different.
    if (m_notificationHandler)
        return false;
    m_notificationHandler = [[AccessibilityNotificationHandler alloc] init];
    [m_notificationHandler setPlatformElement:platformUIElement()];
    [m_notificationHandler setCallback:functionCallback];
    [m_notificationHandler startObserving];

    return true;
}

bool AccessibilityUIElement::removeNotificationListener()
{
    // Mac programmers should not be trying to remove a listener that's already removed.
    ASSERT(m_notificationHandler);

    [m_notificationHandler stopObserving];
    [m_notificationHandler release];
    m_notificationHandler = nil;
    
    return true;
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

bool AccessibilityUIElement::isVisible() const
{
    // FIXME: implement
    return false;
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
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:@"AXHasPopup"];
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::popupValue() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id value = [m_element accessibilityAttributeValue:@"AXPopupValue"];
    if ([value isKindOfClass:[NSString class]])
        return [value createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return [@"false" createJSStringRef];
}

void AccessibilityUIElement::takeFocus()
{
    setAttributeValue(m_element.get(), NSAccessibilityFocusedAttribute, @YES);
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

// Text markers
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXLineTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange::create(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::misspellingTextMarkerRange(AccessibilityTextMarkerRange* start, bool forward)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameters = misspellingSearchParameterizedAttributeForCriteria(start, forward);
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXMisspellingTextMarkerRange" forParameter:parameters];
    return AccessibilityTextMarkerRange::create(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForElement(AccessibilityUIElement* element)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXTextMarkerRangeForUIElement" forParameter:element->platformUIElement()];
    return AccessibilityTextMarkerRange::create(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

int AccessibilityUIElement::textMarkerRangeLength(AccessibilityTextMarkerRange* range)
{
    if (!range)
        return 0;

    BEGIN_AX_OBJC_EXCEPTIONS
    NSNumber* lengthValue = [m_element accessibilityAttributeValue:@"AXLengthForTextMarkerRange" forParameter:range->platformTextMarkerRange()];
    return [lengthValue intValue];
    END_AX_OBJC_EXCEPTIONS
    
    return 0;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id previousMarker = [m_element accessibilityAttributeValue:@"AXPreviousTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker::create(previousMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id nextMarker = [m_element accessibilityAttributeValue:@"AXNextTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker::create(nextMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    if (!markerRange)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id textString = [m_element accessibilityAttributeValue:@"AXStringForTextMarkerRange" forParameter:markerRange->platformTextMarkerRange()];
    return [textString createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id startPlatformMarker = startMarker->platformTextMarker();
    id endPlatformMarker = endMarker->platformTextMarker();
    if (!startPlatformMarker || !endPlatformMarker)
        return nullptr;

    NSArray* textMarkers = @[startPlatformMarker, endPlatformMarker];
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXTextMarkerRangeForUnorderedTextMarkers" forParameter:textMarkers];
    return AccessibilityTextMarkerRange::create(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::selectedTextMarkerRange()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = attributeValue(m_element.get(), NSAccessibilitySelectedTextMarkerRangeAttribute);
    return AccessibilityTextMarkerRange::create(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

void AccessibilityUIElement::resetSelectedTextMarkerRange()
{
    id start = attributeValue(m_element.get(), @"AXStartTextMarker");
    if (!start)
        return;

    NSArray* textMarkers = @[start, start];
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXTextMarkerRangeForUnorderedTextMarkers" forParameter:textMarkers];
    if (!textMarkerRange)
        return;

    setAttributeValue(m_element.get(), NSAccessibilitySelectedTextMarkerRangeAttribute, textMarkerRange, true);
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    if (!range)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXStartTextMarkerForTextMarkerRange" forParameter:range->platformTextMarkerRange()];
    return AccessibilityTextMarker::create(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    if (!range)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXEndTextMarkerForTextMarkerRange" forParameter:range->platformTextMarkerRange()];
    return AccessibilityTextMarker::create(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForBounds(int x, int y, int width, int height)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute forParameter:[NSValue valueWithRect:NSMakeRect(x, y, width, height)]];
    return AccessibilityTextMarker::create(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
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

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForBounds(int x, int y, int width, int height)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute forParameter:[NSValue valueWithRect:NSMakeRect(x, y, width, height)]];
    return AccessibilityTextMarker::create(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForPoint(int x, int y)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXTextMarkerForPosition" forParameter:[NSValue valueWithPoint:NSMakePoint(x, y)]];
    return AccessibilityTextMarker::create(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::accessibilityElementForTextMarker(AccessibilityTextMarker* marker)
{
    if (!marker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id uiElement = [m_element accessibilityAttributeValue:@"AXUIElementForTextMarker" forParameter:marker->platformTextMarker()];
    if (uiElement)
        return AccessibilityUIElement::create(uiElement);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

static JSRetainPtr<JSStringRef> createJSStringRef(id string)
{
    id mutableString = [[[NSMutableString alloc] init] autorelease];
    id attributes = [string attributesAtIndex:0 effectiveRange:nil];
    id attributeEnumerationBlock = ^(NSDictionary<NSString *, id> *attrs, NSRange range, BOOL *stop) {
        BOOL misspelled = [[attrs objectForKey:NSAccessibilityMisspelledTextAttribute] boolValue];
        if (misspelled)
            misspelled = [[attrs objectForKey:NSAccessibilityMarkedMisspelledTextAttribute] boolValue];
        if (misspelled)
            [mutableString appendString:@"Misspelled, "];
        id font = [attributes objectForKey:(__bridge id)kAXFontTextAttribute];
        if (font)
            [mutableString appendFormat:@"%@ - %@, ", (__bridge id)kAXFontTextAttribute, font];
    };
    [string enumerateAttributesInRange:NSMakeRange(0, [string length]) options:(NSAttributedStringEnumerationOptions)0 usingBlock:attributeEnumerationBlock];
    [mutableString appendString:[string string]];
    return [mutableString createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    if (!markerRange)
        return nullptr;

    NSAttributedString* string = nil;

    BEGIN_AX_OBJC_EXCEPTIONS
    string = [m_element accessibilityAttributeValue:@"AXAttributedStringForTextMarkerRange" forParameter:markerRange->platformTextMarkerRange()];
    END_AX_OBJC_EXCEPTIONS

    if (![string isKindOfClass:[NSAttributedString class]])
        return nil;

    return createJSStringRef(string);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange* markerRange, bool includeSpellCheck)
{
    if (!markerRange || !markerRange->platformTextMarkerRange())
        return nullptr;

    NSAttributedString* string = nil;

    id parameter = nil;
    if (includeSpellCheck)
        parameter = @{ @"AXSpellCheck": includeSpellCheck ? @YES : @NO, @"AXTextMarkerRange": markerRange->platformTextMarkerRange() };
    else
        parameter = markerRange->platformTextMarkerRange();

    BEGIN_AX_OBJC_EXCEPTIONS
    string = [m_element accessibilityAttributeValue:@"AXAttributedStringForTextMarkerRangeWithOptions" forParameter:parameter];
    END_AX_OBJC_EXCEPTIONS

    if (![string isKindOfClass:[NSAttributedString class]])
        return nil;

    return createJSStringRef(string);
}

bool AccessibilityUIElement::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef attribute, AccessibilityTextMarkerRange* range)
{
    if (!range)
        return false;

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
    if (!marker)
        return -1;

    BEGIN_AX_OBJC_EXCEPTIONS
    NSNumber* indexNumber = [m_element accessibilityAttributeValue:@"AXIndexForTextMarker" forParameter:marker->platformTextMarker()];
    return [indexNumber intValue];
    END_AX_OBJC_EXCEPTIONS
    
    return -1;
}

bool AccessibilityUIElement::isTextMarkerValid(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return false;

    BEGIN_AX_OBJC_EXCEPTIONS
    NSNumber* validNumber = [m_element accessibilityAttributeValue:@"AXTextMarkerIsValid" forParameter:textMarker->platformTextMarker()];
    return [validNumber boolValue];
    END_AX_OBJC_EXCEPTIONS
    
    return false;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForIndex(int textIndex)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXTextMarkerForIndex" forParameter:[NSNumber numberWithInteger:textIndex]];
    return AccessibilityTextMarker::create(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}
    
RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarker()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXStartTextMarker"];
    return AccessibilityTextMarker::create(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarker()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarker = [m_element accessibilityAttributeValue:@"AXEndTextMarker"];
    return AccessibilityTextMarker::create(textMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::leftWordTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXLeftWordTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange::create(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::rightWordTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXRightWordTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange::create(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousWordStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id previousWordStartMarker = [m_element accessibilityAttributeValue:@"AXPreviousWordStartTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker::create(previousWordStartMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextWordEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id nextWordEndMarker = [m_element accessibilityAttributeValue:@"AXNextWordEndTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker::create(nextWordEndMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::paragraphTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXParagraphTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange::create(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousParagraphStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id previousParagraphStartMarker = [m_element accessibilityAttributeValue:@"AXPreviousParagraphStartTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker::create(previousParagraphStartMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextParagraphEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id nextParagraphEndMarker = [m_element accessibilityAttributeValue:@"AXNextParagraphEndTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker::create(nextParagraphEndMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::sentenceTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id textMarkerRange = [m_element accessibilityAttributeValue:@"AXSentenceTextMarkerRangeForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarkerRange::create(textMarkerRange);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousSentenceStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id previousParagraphStartMarker = [m_element accessibilityAttributeValue:@"AXPreviousSentenceStartTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker::create(previousParagraphStartMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextSentenceEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    id nextParagraphEndMarker = [m_element accessibilityAttributeValue:@"AXNextSentenceEndTextMarkerForTextMarker" forParameter:textMarker->platformTextMarker()];
    return AccessibilityTextMarker::create(nextParagraphEndMarker);
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

static NSString *_convertMathMultiscriptPairsToString(NSArray *pairs)
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
    return [_convertMathMultiscriptPairsToString(pairs) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPrescriptsDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray *pairs = [m_element accessibilityAttributeValue:@"AXMathPrescripts"];
    return [_convertMathMultiscriptPairsToString(pairs) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}
    
JSRetainPtr<JSStringRef> AccessibilityUIElement::pathDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSMutableString *result = [NSMutableString stringWithString:@"\nStart Path\n"];
    NSBezierPath *bezierPath = [m_element accessibilityAttributeValue:NSAccessibilityPathAttribute];
    
    NSUInteger elementCount = [bezierPath elementCount];
    NSPoint points[3];
    for (NSUInteger i = 0; i < elementCount; i++) {
        switch ([bezierPath elementAtIndex:i associatedPoints:points]) {
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
    
JSRetainPtr<JSStringRef> AccessibilityUIElement::supportedActions() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray *names = [m_element accessibilityActionNames];
    return [[names componentsJoinedByString:@","] createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

} // namespace WTR

#endif // HAVE(ACCESSIBILITY)
