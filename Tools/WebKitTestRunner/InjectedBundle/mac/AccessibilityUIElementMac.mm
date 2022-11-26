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
#import "AccessibilityCommonCocoa.h"

#if ENABLE(ACCESSIBILITY)

#import "AccessibilityNotificationHandler.h"
#import "AccessibilityUIElement.h"
#import "InjectedBundle.h"
#import "InjectedBundlePage.h"
#import "JSBasics.h"
#import <AppKit/NSAccessibility.h>
#import <Foundation/Foundation.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <JavaScriptCore/JSObjectRef.h>
#import <WebKit/WKBundleFrame.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/VectorCocoa.h>

#if HAVE(ACCESSIBILITY_FRAMEWORK)
#import <Accessibility/Accessibility.h>
#endif

#define NSAccessibilityDOMIdentifierAttribute @"AXDOMIdentifier"

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

#ifndef NSAccessibilityARIACurrentAttribute
#define NSAccessibilityARIACurrentAttribute @"AXARIACurrent"
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

static id attributeValue(id element, NSString *attribute)
{
    // The given `element` may not respond to `accessibilityAttributeValue`, so first check to see if it responds to the attribute-specific selector.
    if ([attribute isEqual:NSAccessibilityChildrenAttribute] && [element respondsToSelector:@selector(accessibilityChildren)])
        return [element accessibilityChildren];
    if ([attribute isEqual:NSAccessibilityDescriptionAttribute] && [element respondsToSelector:@selector(accessibilityLabel)])
        return [element accessibilityLabel];
    if ([attribute isEqual:NSAccessibilityParentAttribute] && [element respondsToSelector:@selector(accessibilityParent)])
        return [element accessibilityParent];
    if ([attribute isEqual:NSAccessibilityRoleAttribute] && [element respondsToSelector:@selector(accessibilityRole)])
        return [element accessibilityRole];
    if ([attribute isEqual:NSAccessibilityValueAttribute] && [element respondsToSelector:@selector(accessibilityValue)])
        return [element accessibilityValue];
    if ([attribute isEqual:NSAccessibilityFocusedUIElementAttribute] && [element respondsToSelector:@selector(accessibilityFocusedUIElement)])
        return [element accessibilityFocusedUIElement];

    return [element accessibilityAttributeValue:attribute];
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

RetainPtr<NSString> AccessibilityUIElement::descriptionOfValue(id valueObject) const
{
    if (!valueObject)
        return nil;

    if ([valueObject isKindOfClass:[NSArray class]])
        return [NSString stringWithFormat:@"<array of size %lu>", static_cast<unsigned long>([(NSArray*)valueObject count])];

    if ([valueObject isKindOfClass:[NSNumber class]])
        return [(NSNumber*)valueObject stringValue];

    if ([valueObject isKindOfClass:[NSValue class]]) {
        NSString *type = [NSString stringWithCString:[valueObject objCType] encoding:NSASCIIStringEncoding];
        NSValue *value = (NSValue *)valueObject;
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
    NSString *description = [valueObject description];
    NSRange range = [description rangeOfString:@"LayoutTests"];
    if (range.length)
        return [description substringFromIndex:range.location];

    // Strip pointer locations
    if ([description rangeOfString:@"0x"].length) {
        auto role = attributeValue(NSAccessibilityRoleAttribute);
        auto title = attributeValue(NSAccessibilityTitleAttribute);
        if ([title length])
            return [NSString stringWithFormat:@"<%@: '%@'>", role.get(), title.get()];
        return [NSString stringWithFormat:@"<%@>", role.get()];
    }

    return [valueObject description];
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

static JSRetainPtr<JSStringRef> descriptionOfElements(const Vector<RefPtr<AccessibilityUIElement>>& elements)
{
    NSMutableString *allElementString = [NSMutableString string];
    for (auto element : elements) {
        NSString *attributes = [NSString stringWithJSStringRef:element->allAttributes().get()];
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
        if (JSValueIsString(context, searchStrings))
            [searchStringsParameter addObject:toWTFString(context, searchStrings)];
        else {
            JSObjectRef searchStringsArray = JSValueToObject(context, searchStrings, nullptr);
            unsigned searchStringsArrayLength = arrayLength(context, searchStringsArray);
            for (unsigned i = 0; i < searchStringsArrayLength; ++i)
                [searchStringsParameter addObject:toWTFString(context, JSObjectGetPropertyAtIndex(context, searchStringsArray, i, nullptr))];
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
        if (JSValueIsString(context, searchStrings))
            [searchStringsParameter addObject:toWTFString(context, searchStrings)];
        else {
            JSObjectRef searchStringsArray = JSValueToObject(context, searchStrings, nullptr);
            unsigned searchStringsArrayLength = arrayLength(context, searchStringsArray);
            for (unsigned i = 0; i < searchStringsArrayLength; ++i)
                [searchStringsParameter addObject:toWTFString(context, JSObjectGetPropertyAtIndex(context, searchStringsArray, i, nullptr))];
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

RetainPtr<id> AccessibilityUIElement::attributeValue(NSString *attributeName) const
{
    RetainPtr<id> value;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([this, &attributeName, &value] {
        value = WTR::attributeValue(m_element.getAutoreleased(), attributeName);
    });
    END_AX_OBJC_EXCEPTIONS

    return value;
}

RetainPtr<id> AccessibilityUIElement::attributeValueForParameter(NSString *attributeName, id parameter) const
{
    RetainPtr<id> value;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([this, &attributeName, &parameter, &value] {
        value = [m_element accessibilityAttributeValue:attributeName forParameter:parameter];
    });
    END_AX_OBJC_EXCEPTIONS

    return value;
}

unsigned AccessibilityUIElement::arrayAttributeCount(NSString *attributeName) const
{
    unsigned count = 0;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([&attributeName, &count, this] {
        count = [m_element accessibilityArrayAttributeCount:attributeName];
    });
    END_AX_OBJC_EXCEPTIONS

    return count;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::domIdentifier() const
{
    return stringAttributeValue(NSAccessibilityDOMIdentifierAttribute);
}

void AccessibilityUIElement::getLinkedUIElements(Vector<RefPtr<AccessibilityUIElement>>& elementVector)
{
    elementVector = makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(NSAccessibilityLinkedUIElementsAttribute).get());
}

void AccessibilityUIElement::getDocumentLinks(Vector<RefPtr<AccessibilityUIElement>>& elementVector)
{
    elementVector = makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(@"AXLinkUIElements").get());
}

void AccessibilityUIElement::getUIElementsWithAttribute(JSStringRef attribute, Vector<RefPtr<AccessibilityUIElement>>& elements) const
{
    auto value = attributeValue([NSString stringWithJSStringRef:attribute]);
    if ([value isKindOfClass:[NSArray class]])
        elements = makeVector<RefPtr<AccessibilityUIElement>>(value.get());
}

JSValueRef AccessibilityUIElement::children() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return makeJSArray(makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(NSAccessibilityChildrenAttribute).get()));
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

void AccessibilityUIElement::getChildren(Vector<RefPtr<AccessibilityUIElement> >& elementVector)
{
    elementVector = makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(NSAccessibilityChildrenAttribute).get());
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

JSRetainPtr<JSStringRef> AccessibilityUIElement::customContent() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
#if HAVE(ACCESSIBILITY_FRAMEWORK)
    auto customContent = adoptNS([[NSMutableArray alloc] init]);
    s_controller->executeOnAXThreadAndWait([this, &customContent] {
        for (AXCustomContent *content in [m_element accessibilityCustomContent])
            [customContent addObject:[NSString stringWithFormat:@"%@: %@", content.label, content.value]];
    });

    return [[customContent.get() componentsJoinedByString:@"\n"] createJSStringRef];
#else
    return nullptr;
#endif
    END_AX_OBJC_EXCEPTIONS
}

JSValueRef AccessibilityUIElement::rowHeaders() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    Vector<RefPtr<AccessibilityUIElement>> elements;
    auto value = attributeValue(NSAccessibilityRowHeaderUIElementsAttribute);
    if ([value isKindOfClass:[NSArray class]])
        elements = makeVector<RefPtr<AccessibilityUIElement>>(value.get());
    return makeJSArray(elements);
    END_AX_OBJC_EXCEPTIONS
}

JSValueRef AccessibilityUIElement::columnHeaders() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    Vector<RefPtr<AccessibilityUIElement>> elements;
    auto value = attributeValue(NSAccessibilityColumnHeaderUIElementsAttribute);
    if ([value isKindOfClass:[NSArray class]])
        elements = makeVector<RefPtr<AccessibilityUIElement>>(value.get());
    return makeJSArray(elements);
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
    auto element = attributeValue(attribute);
    return element ? AccessibilityUIElement::create(element.get()) : RefPtr<AccessibilityUIElement>();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::elementForAttributeAtIndex(NSString* attribute, unsigned index) const
{
    auto elements = attributeValue(attribute);
    return index < [elements count] ? AccessibilityUIElement::create([elements objectAtIndex:index]) : RefPtr<AccessibilityUIElement>();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::linkedUIElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(NSAccessibilityLinkedUIElementsAttribute, index);
}

JSValueRef AccessibilityUIElement::detailsElements() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto elements = attributeValue(@"AXDetailsElements");
    if ([elements isKindOfClass:NSArray.class])
        return makeJSArray(makeVector<RefPtr<AccessibilityUIElement>>(elements.get()));
    END_AX_OBJC_EXCEPTIONS

    return { };
}

JSValueRef AccessibilityUIElement::errorMessageElements() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto elements = attributeValue(@"AXErrorMessageElements");
    if ([elements isKindOfClass:NSArray.class])
        return makeJSArray(makeVector<RefPtr<AccessibilityUIElement>>(elements.get()));
    END_AX_OBJC_EXCEPTIONS

    return { };
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
    return arrayAttributeCount(NSAccessibilitySelectedChildrenAttribute);
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
    auto attributes = supportedAttributes(m_element.getAutoreleased());

    NSMutableString *values = [NSMutableString string];
    for (NSString *attribute in attributes.get()) {
        // Exclude screen-specific values, since they can change depending on the system.
        if ([attribute isEqualToString:@"AXPosition"]
            || [attribute isEqualToString:@"_AXPrimaryScreenHeight"]
            || [attribute isEqualToString:@"AXRelativeFrame"])
            continue;

        auto value = descriptionOfValue(attributeValue(attribute).get());
        [values appendFormat:@"%@: %@\n", attribute, value.get()];
    }

    return [values createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringDescriptionOfAttributeValue(JSStringRef attribute)
{
    auto value = attributeValue([NSString stringWithJSStringRef:attribute]);
    auto valueDescription = descriptionOfValue(value.get());
    return [valueDescription createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(JSStringRef attribute)
{
    return stringAttributeValue([NSString stringWithJSStringRef:attribute]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(NSString *attribute) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(attribute);
    if ([value isKindOfClass:[NSString class]])
        return [value createJSStringRef];
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
    auto value = attributeValue(attribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [value doubleValue];
    END_AX_OBJC_EXCEPTIONS

    return 0;
}

JSValueRef AccessibilityUIElement::uiElementArrayAttributeValue(JSStringRef attribute) const
{
    Vector<RefPtr<AccessibilityUIElement>> elements;
    getUIElementsWithAttribute(attribute, elements);
    return makeJSArray(elements);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementAttributeValue(JSStringRef attribute) const
{
    if (auto value = attributeValue([NSString stringWithJSStringRef:attribute]))
        return AccessibilityUIElement::create(value.get());
    return nullptr;
}

bool AccessibilityUIElement::boolAttributeValue(NSString *attribute) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(attribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

bool AccessibilityUIElement::boolAttributeValue(JSStringRef attribute)
{
    return boolAttributeValue([NSString stringWithJSStringRef:attribute]);
}

void AccessibilityUIElement::attributeValueAsync(JSStringRef attribute, JSValueRef callback)
{
    if (!attribute || !callback)
        return;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThread([attribute = retainPtr([NSString stringWithJSStringRef:attribute]), callback = WTFMove(callback), this] () mutable {
        id value = [m_element accessibilityAttributeValue:attribute.get()];
        if ([value isKindOfClass:[NSArray class]] || [value isKindOfClass:[NSDictionary class]])
            value = [value description];
        
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
    setAttributeValue(m_element.getAutoreleased(), [NSString stringWithJSStringRef:attribute], @(value), true);
}

void AccessibilityUIElement::setValue(JSStringRef value)
{
    setAttributeValue(m_element.getAutoreleased(), NSAccessibilityValueAttribute, [NSString stringWithJSStringRef:value]);
}

bool AccessibilityUIElement::isAttributeSettable(JSStringRef attribute)
{
    return isAttributeSettable([NSString stringWithJSStringRef:attribute]);
}

bool AccessibilityUIElement::isAttributeSettable(NSString *attribute) const
{
    bool value = false;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([this, &attribute, &value] {
        value = [m_element accessibilityIsAttributeSettable:attribute];
    });
    END_AX_OBJC_EXCEPTIONS

    return value;
}

bool AccessibilityUIElement::isAttributeSupported(JSStringRef attribute)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [supportedAttributes(m_element.getAutoreleased()) containsObject:[NSString stringWithJSStringRef:attribute]];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::parameterizedAttributeNames()
{
    NSArray *attributes = nil;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([&attributes, this] {
        attributes = [m_element accessibilityParameterizedAttributeNames];
    });
    END_AX_OBJC_EXCEPTIONS

    NSMutableString *attributesString = [NSMutableString string];
    for (id attribute in attributes)
        [attributesString appendFormat:@"%@\n", attribute];
    return [attributesString createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::role()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto role = descriptionOfValue(attributeValue(NSAccessibilityRoleAttribute).get());
    return concatenateAttributeAndValue(@"AXRole", role.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::subrole()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto subrole = descriptionOfValue(attributeValue(NSAccessibilitySubroleAttribute).get());
    return concatenateAttributeAndValue(@"AXSubrole", subrole.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::roleDescription()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto role = descriptionOfValue(attributeValue(NSAccessibilityRoleDescriptionAttribute).get());
    return concatenateAttributeAndValue(@"AXRoleDescription", role.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::computedRoleString()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto computedRoleString = descriptionOfValue(attributeValue(@"AXARIARole").get());
    return [computedRoleString createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::title()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto title = descriptionOfValue(attributeValue(NSAccessibilityTitleAttribute).get());
    return concatenateAttributeAndValue(@"AXTitle", title.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::description()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto description = descriptionOfValue(attributeValue(NSAccessibilityDescriptionAttribute).get());
    return concatenateAttributeAndValue(@"AXDescription", description.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::liveRegionStatus() const
{
    return stringAttributeValue(@"AXARIALive");
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::liveRegionRelevant() const
{
    return stringAttributeValue(@"AXARIARelevant");
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::orientation() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto description = descriptionOfValue(attributeValue(NSAccessibilityOrientationAttribute).get());
    return concatenateAttributeAndValue(@"AXOrientation", description.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringValue()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(NSAccessibilityValueAttribute);
    auto description = descriptionOfValue(value.get());
    if (description)
        return concatenateAttributeAndValue(@"AXValue", description.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::language()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto description = descriptionOfValue(attributeValue(@"AXLanguage").get());
    return concatenateAttributeAndValue(@"AXLanguage", description.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::helpText() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto description = descriptionOfValue(attributeValue(NSAccessibilityHelpAttribute).get());
    return concatenateAttributeAndValue(@"AXHelp", description.get());
    END_AX_OBJC_EXCEPTIONS
    
    return nullptr;
}

double AccessibilityUIElement::x()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto positionValue = attributeValue(NSAccessibilityPositionAttribute);
    return static_cast<double>([positionValue pointValue].x);    
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElement::y()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto positionValue = attributeValue(NSAccessibilityPositionAttribute);
    return static_cast<double>([positionValue pointValue].y);    
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElement::width()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto sizeValue = attributeValue(NSAccessibilitySizeAttribute);
    return static_cast<double>([sizeValue sizeValue].width);
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElement::height()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto sizeValue = attributeValue(NSAccessibilitySizeAttribute);
    return static_cast<double>([sizeValue sizeValue].height);
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElement::clickPointX()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto positionValue = attributeValue(@"AXClickPoint");
    return static_cast<double>([positionValue pointValue].x);        
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElement::clickPointY()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto positionValue = attributeValue(@"AXClickPoint");
    return static_cast<double>([positionValue pointValue].y);
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::lineRectsAndText() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto lineRectsAndText = attributeValue(@"AXLineRectsAndText");
    if (![lineRectsAndText isKindOfClass:NSArray.class])
        return { };
    return [[lineRectsAndText componentsJoinedByString:@"|"] createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return { };
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
    auto valueDescription = attributeValue(NSAccessibilityValueDescriptionAttribute);
    if ([valueDescription isKindOfClass:[NSString class]])
        return concatenateAttributeAndValue(@"AXValueDescription", valueDescription.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(NSAccessibilityInsertionPointLineNumberAttribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber *)value intValue]; 
    END_AX_OBJC_EXCEPTIONS

    return -1;
}

bool AccessibilityUIElement::isPressActionSupported()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [actionNames() containsObject:NSAccessibilityPressAction];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

bool AccessibilityUIElement::isIncrementActionSupported()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [actionNames() containsObject:NSAccessibilityIncrementAction];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

bool AccessibilityUIElement::isDecrementActionSupported()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [actionNames() containsObject:NSAccessibilityDecrementAction];
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

RefPtr<AccessibilityUIElement> AccessibilityUIElement::focusedElement() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    if (auto focus = attributeValue(NSAccessibilityFocusedUIElementAttribute))
        return AccessibilityUIElement::create(focus.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

bool AccessibilityUIElement::isFocused() const
{
    return boolAttributeValue(NSAccessibilityFocusedAttribute);
}

bool AccessibilityUIElement::isSelected() const
{
    auto value = attributeValue(NSAccessibilitySelectedAttribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    return false;
}

bool AccessibilityUIElement::isSelectedOptionActive() const
{
    return false;
}

bool AccessibilityUIElement::isIndeterminate() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(NSAccessibilityValueAttribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [value intValue] == 2;
    END_AX_OBJC_EXCEPTIONS

    return false;
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

JSRetainPtr<JSStringRef> AccessibilityUIElement::currentStateValue() const
{
    return stringAttributeValue(NSAccessibilityARIACurrentAttribute);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::sortDirection() const
{
    return stringAttributeValue(NSAccessibilitySortDirectionAttribute);
}

int AccessibilityUIElement::hierarchicalLevel() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(NSAccessibilityDisclosureLevelAttribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [value intValue];
    END_AX_OBJC_EXCEPTIONS

    return 0;
}
    
JSRetainPtr<JSStringRef> AccessibilityUIElement::classList() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(@"AXDOMClassList");
    if (![value isKindOfClass:[NSArray class]])
        return nullptr;

    NSMutableString *classList = [NSMutableString string];
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
    auto value = attributeValue(@"AXDRTSpeechAttribute");
    if ([value isKindOfClass:[NSArray class]])
        return [[(NSArray *)value componentsJoinedByString:@", "] createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

bool AccessibilityUIElement::ariaIsGrabbed() const
{
    return boolAttributeValue(NSAccessibilityGrabbedAttribute);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::ariaDropEffects() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(NSAccessibilityDropEffectsAttribute);
    if (![value isKindOfClass:[NSArray class]])
        return nullptr;

    NSMutableString *dropEffects = [NSMutableString string];
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
    auto value = attributeValueForParameter(NSAccessibilityLineForIndexParameterizedAttribute, @(index));
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber *)value intValue]; 
    END_AX_OBJC_EXCEPTIONS

    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForLine(int line)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValueForParameter(NSAccessibilityRangeForLineParameterizedAttribute, @(line));
    if ([value isKindOfClass:[NSValue class]])
        return [NSStringFromRange([value rangeValue]) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForPosition(int x, int y)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValueForParameter(NSAccessibilityRangeForPositionParameterizedAttribute, [NSValue valueWithPoint:NSMakePoint(x, y)]);
    if ([value isKindOfClass:[NSValue class]])
        return [NSStringFromRange([value rangeValue]) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::boundsForRange(unsigned location, unsigned length)
{
    NSRange range = NSMakeRange(location, length);
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValueForParameter(NSAccessibilityBoundsForRangeParameterizedAttribute, [NSValue valueWithRange:range]);
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
    auto string = attributeValueForParameter(NSAccessibilityStringForRangeParameterizedAttribute, [NSValue valueWithRange:range]);
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
    auto string = attributeValueForParameter(NSAccessibilityAttributedStringForRangeParameterizedAttribute, [NSValue valueWithRange:range]);
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
    auto string = attributeValueForParameter(NSAccessibilityAttributedStringForRangeParameterizedAttribute, [NSValue valueWithRange:range]);
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
    auto value = attributeValueForParameter(@"AXUIElementCountForSearchPredicate", parameterizedAttribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [value unsignedIntValue];
    END_AX_OBJC_EXCEPTIONS
    
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement *startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameterizedAttribute = searchPredicateParameterizedAttributeForSearchCriteria(context, startElement, isDirectionNext, 1, searchKey, searchText, visibleOnly, immediateDescendantsOnly);
    auto searchResults = attributeValueForParameter(@"AXUIElementsForSearchPredicate", parameterizedAttribute);
    if ([searchResults isKindOfClass:[NSArray class]]) {
        if (id lastResult = [searchResults lastObject])
            return AccessibilityUIElement::create(lastResult);
    }
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectTextWithCriteria(JSContextRef context, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameterizedAttribute = selectTextParameterizedAttributeForCriteria(context, ambiguityResolution, searchStrings, replacementString, activity);
    auto result = attributeValueForParameter(@"AXSelectTextWithCriteria", parameterizedAttribute);
    if ([result isKindOfClass:[NSString class]])
        return [result.get() createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

#if PLATFORM(MAC)
JSValueRef AccessibilityUIElement::searchTextWithCriteria(JSContextRef context, JSValueRef searchStrings, JSStringRef startFrom, JSStringRef direction)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameterizedAttribute = searchTextParameterizedAttributeForCriteria(context, searchStrings, startFrom, direction);
    auto result = attributeValueForParameter(@"AXSearchTextWithCriteria", parameterizedAttribute);
    if ([result isKindOfClass:[NSArray class]])
        return makeJSArray(makeVector<RefPtr<AccessibilityTextMarkerRange>>(result.get()));
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}
#endif

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumnHeaders()
{
    // not yet defined in AppKit... odd
    BEGIN_AX_OBJC_EXCEPTIONS
    auto columnHeaders = attributeValue(@"AXColumnHeaderUIElements");
    auto columnHeadersVector = makeVector<RefPtr<AccessibilityUIElement>>(columnHeaders.get());
    return descriptionOfElements(columnHeadersVector);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRowHeaders()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto rowHeaders = attributeValue(@"AXRowHeaderUIElements");
    auto rowHeadersVector = makeVector<RefPtr<AccessibilityUIElement>>(rowHeaders.get());
    return descriptionOfElements(rowHeadersVector);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumns()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto columns = attributeValue(NSAccessibilityColumnsAttribute);
    auto columnsVector = makeVector<RefPtr<AccessibilityUIElement>>(columns.get());
    return descriptionOfElements(columnsVector);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRows()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto rows = attributeValue(NSAccessibilityRowsAttribute);
    auto rowsVector = makeVector<RefPtr<AccessibilityUIElement>>(rows.get());
    return descriptionOfElements(rowsVector);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfVisibleCells()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto cells = attributeValue(@"AXVisibleCells");
    auto cellsVector = makeVector<RefPtr<AccessibilityUIElement>>(cells.get());
    return descriptionOfElements(cellsVector);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfHeader()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto header = attributeValue(NSAccessibilityHeaderAttribute);
    if (!header)
        return [@"" createJSStringRef];

    Vector<RefPtr<AccessibilityUIElement>> headerVector;
    headerVector.append(AccessibilityUIElement::create(header.get()));
    return descriptionOfElements(headerVector);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

int AccessibilityUIElement::rowCount()
{
    return arrayAttributeCount(NSAccessibilityRowsAttribute);
}

int AccessibilityUIElement::columnCount()
{
    return arrayAttributeCount(NSAccessibilityColumnsAttribute);
}

int AccessibilityUIElement::indexInTable()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto indexNumber = attributeValue(NSAccessibilityIndexAttribute);
    if (indexNumber)
        return [indexNumber intValue];
    END_AX_OBJC_EXCEPTIONS

    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rowIndexRange()
{
    NSRange range = NSMakeRange(0, 0);
    BEGIN_AX_OBJC_EXCEPTIONS
    auto indexRange = attributeValue(@"AXRowIndexRange");
    if (indexRange)
        range = [indexRange rangeValue];
    NSMutableString *rangeDescription = [NSMutableString stringWithFormat:@"{%lu, %lu}", static_cast<unsigned long>(range.location), static_cast<unsigned long>(range.length)];
    return [rangeDescription createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::columnIndexRange()
{
    NSRange range = NSMakeRange(0, 0);
    BEGIN_AX_OBJC_EXCEPTIONS
    auto indexRange = attributeValue(@"AXColumnIndexRange");
    if (indexRange)
        range = [indexRange rangeValue];
    NSMutableString *rangeDescription = [NSMutableString stringWithFormat:@"{%lu, %lu}", static_cast<unsigned long>(range.location), static_cast<unsigned long>(range.length)];
    return [rangeDescription createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::cellForColumnAndRow(unsigned col, unsigned row)
{
    NSArray *colRowArray = @[@(col), @(row)];
    BEGIN_AX_OBJC_EXCEPTIONS
    if (auto cell = attributeValueForParameter(@"AXCellForColumnAndRow", colRowArray))
        return AccessibilityUIElement::create(cell.get());
    END_AX_OBJC_EXCEPTIONS    

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::horizontalScrollbar() const
{
    if (!m_element)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    if (id scrollbar = attributeValue(NSAccessibilityHorizontalScrollBarAttribute).get())
        return AccessibilityUIElement::create(scrollbar);
    END_AX_OBJC_EXCEPTIONS    

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::verticalScrollbar() const
{
    if (!m_element)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    if (id scrollbar = attributeValue(NSAccessibilityVerticalScrollBarAttribute).get())
        return AccessibilityUIElement::create(scrollbar);
    END_AX_OBJC_EXCEPTIONS        

    return nullptr;
}

void AccessibilityUIElement::scrollToMakeVisible()
{
    performAction(@"AXScrollToVisible");
}

void AccessibilityUIElement::scrollToGlobalPoint(int x, int y)
{
    NSPoint point = NSMakePoint(x, y);
    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThread([&point, this] {
        [m_element _accessibilityScrollToGlobalPoint:point];
    });
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height)
{
    NSRect rect = NSMakeRect(x, y, width, height);
    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThread([&rect, this] {
        [m_element _accessibilityScrollToMakeVisibleWithSubFocus:rect];
    });
    END_AX_OBJC_EXCEPTIONS
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectedTextRange()
{
    NSRange range = NSMakeRange(NSNotFound, 0);
    BEGIN_AX_OBJC_EXCEPTIONS
    auto indexRange = attributeValue(NSAccessibilitySelectedTextRangeAttribute);
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
    setAttributeValue(m_element.getAutoreleased(), NSAccessibilitySelectedTextRangeAttribute, textRangeValue);

    return true;
}

void AccessibilityUIElement::dismiss()
{
    performAction(@"AXDismissAction");
}

bool AccessibilityUIElement::setSelectedTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    if (!markerRange)
        return false;

    setAttributeValue(m_element.getAutoreleased(), NSAccessibilitySelectedTextMarkerRangeAttribute, markerRange->platformTextMarkerRange());

    return true;
}

void AccessibilityUIElement::increment()
{
    performAction(@"AXSyncIncrementAction");
}

void AccessibilityUIElement::decrement()
{
    performAction(@"AXSyncDecrementAction");
}

void AccessibilityUIElement::asyncIncrement()
{
    performAction(NSAccessibilityIncrementAction);
}

void AccessibilityUIElement::asyncDecrement()
{
    performAction(NSAccessibilityDecrementAction);
}

void AccessibilityUIElement::showMenu()
{
    performAction(NSAccessibilityShowMenuAction);
}

void AccessibilityUIElement::press()
{
    performAction(NSAccessibilityPressAction);
}

void AccessibilityUIElement::syncPress()
{
    performAction(@"AXSyncPressAction");
}

void AccessibilityUIElement::setSelectedChild(AccessibilityUIElement* element) const
{
    NSArray* array = element ? @[element->platformUIElement()] : @[];
    setAttributeValue(m_element.getAutoreleased(), NSAccessibilitySelectedChildrenAttribute, array);
}

void AccessibilityUIElement::setSelectedChildAtIndex(unsigned index) const
{
    RefPtr<AccessibilityUIElement> element = const_cast<AccessibilityUIElement*>(this)->childAtIndex(index);
    if (!element)
        return;

    auto selectedChildren = attributeValue(NSAccessibilitySelectedChildrenAttribute);
    NSArray *array = @[element->platformUIElement()];
    if (selectedChildren)
        array = [selectedChildren arrayByAddingObjectsFromArray:array];

    setAttributeValue(m_element.getAutoreleased(), NSAccessibilitySelectedChildrenAttribute, array, true);
}

void AccessibilityUIElement::removeSelectionAtIndex(unsigned index) const
{
    RefPtr<AccessibilityUIElement> element = const_cast<AccessibilityUIElement*>(this)->childAtIndex(index);
    if (!element)
        return;

    auto selectedChildren = attributeValue(NSAccessibilitySelectedChildrenAttribute);
    if (!selectedChildren)
        return;

    NSMutableArray *array = [NSMutableArray arrayWithArray:selectedChildren.get()];
    [array removeObject:element->platformUIElement()];

    setAttributeValue(m_element.getAutoreleased(), NSAccessibilitySelectedChildrenAttribute, array, true);
}

void AccessibilityUIElement::clearSelectedChildren() const
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::accessibilityValue() const
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentEncoding()
{
    if (auto result = stringAttributeValue(@"AXDocumentEncoding"))
        return result;

    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentURI()
{
    if (auto result = stringAttributeValue(@"AXDocumentURI"))
        return result;

    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::url()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto url = attributeValue(NSAccessibilityURLAttribute);
    if ([url isKindOfClass:[NSURL class]])
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

    m_notificationHandler = adoptNS([[AccessibilityNotificationHandler alloc] init]);
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
    m_notificationHandler = nil;
    
    return true;
}

bool AccessibilityUIElement::isFocusable() const
{
    return isAttributeSettable(NSAccessibilityFocusedAttribute);
}

bool AccessibilityUIElement::isSelectable() const
{
    return isAttributeSettable(NSAccessibilitySelectedAttribute);
}

bool AccessibilityUIElement::isMultiSelectable() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(@"AXIsMultiSelectable");
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    END_AX_OBJC_EXCEPTIONS
    return false;
}

bool AccessibilityUIElement::isVisible() const
{
    return false;
}

bool AccessibilityUIElement::isOnScreen() const
{
    auto value = attributeValue(@"AXIsOnScreen");
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    return false;
}

bool AccessibilityUIElement::isOffScreen() const
{
    return false;
}

bool AccessibilityUIElement::isCollapsed() const
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::embeddedImageDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = descriptionOfValue(attributeValue(@"AXEmbeddedImageDescription").get());
    return concatenateAttributeAndValue(@"AXEmbeddedImageDescription", value.get());
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}

JSValueRef AccessibilityUIElement::imageOverlayElements() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return makeJSArray(makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(@"AXImageOverlayElements").get()));
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

bool AccessibilityUIElement::isIgnored() const
{
    BOOL result = NO;
    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([&result, this] {
        result = [m_element accessibilityIsIgnored];
    });
    END_AX_OBJC_EXCEPTIONS
    return result;
}

bool AccessibilityUIElement::isSingleLine() const
{
    return false;
}

bool AccessibilityUIElement::isMultiLine() const
{
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
    setAttributeValue(m_element.getAutoreleased(), NSAccessibilityFocusedAttribute, @YES);
}

void AccessibilityUIElement::takeSelection()
{
}

void AccessibilityUIElement::addSelection()
{
}

void AccessibilityUIElement::removeSelection()
{
}

// Text markers
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXLineTextMarkerRangeForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

int AccessibilityUIElement::lineIndexForTextMarker(AccessibilityTextMarker* marker) const
{
    if (!marker)
        return -1;

    BEGIN_AX_OBJC_EXCEPTIONS
    return [attributeValueForParameter(@"AXLineForTextMarker", marker->platformTextMarker()) intValue];
    END_AX_OBJC_EXCEPTIONS

    return -1;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::misspellingTextMarkerRange(AccessibilityTextMarkerRange* start, bool forward)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameters = misspellingSearchParameterizedAttributeForCriteria(start, forward);
    auto textMarkerRange = attributeValueForParameter(@"AXMisspellingTextMarkerRange", parameters);
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForElement(AccessibilityUIElement* element)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXTextMarkerRangeForUIElement", element->platformUIElement());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

int AccessibilityUIElement::textMarkerRangeLength(AccessibilityTextMarkerRange* range)
{
    if (!range)
        return 0;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto lengthValue = attributeValueForParameter(@"AXLengthForTextMarkerRange", range->platformTextMarkerRange());
    return [lengthValue intValue];
    END_AX_OBJC_EXCEPTIONS

    return 0;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto previousMarker = attributeValueForParameter(@"AXPreviousTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(previousMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto nextMarker = attributeValueForParameter(@"AXNextTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(nextMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    if (!markerRange)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textString = attributeValueForParameter(@"AXStringForTextMarkerRange", markerRange->platformTextMarkerRange());
    return [textString createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rectsForTextMarkerRange(AccessibilityTextMarkerRange*, JSStringRef)
{
    // Not implemented on macOS
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSArray *textMarkers = @[startMarker->platformTextMarker(), endMarker->platformTextMarker()];
    auto textMarkerRange = attributeValueForParameter(@"AXTextMarkerRangeForTextMarkers", textMarkers);
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::textMarkerRangeForRange(unsigned location, unsigned length)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return AccessibilityTextMarkerRange::create(attributeValueForParameter(@"AXTextMarkerRangeForNSRange",
        [NSValue valueWithRange:NSMakeRange(location, length)]).get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::selectedTextMarkerRange()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValue(NSAccessibilitySelectedTextMarkerRangeAttribute);
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

void AccessibilityUIElement::resetSelectedTextMarkerRange()
{
    auto start = attributeValue(@"AXStartTextMarker");
    if (!start)
        return;

    NSArray *textMarkers = @[start.get(), start.get()];
    auto textMarkerRange = attributeValueForParameter(@"AXTextMarkerRangeForUnorderedTextMarkers", textMarkers);
    if (!textMarkerRange)
        return;

    setAttributeValue(m_element.getAutoreleased(), NSAccessibilitySelectedTextMarkerRangeAttribute, textMarkerRange.get(), true);
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    if (!range)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValueForParameter(@"AXStartTextMarkerForTextMarkerRange", range->platformTextMarkerRange());
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    if (!range)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValueForParameter(@"AXEndTextMarkerForTextMarkerRange", range->platformTextMarkerRange());
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarkerForBounds(int x, int y, int width, int height)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValueForParameter(NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute,
        [NSValue valueWithRect:NSMakeRect(x, y, width, height)]);
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

bool AccessibilityUIElement::replaceTextInRange(JSStringRef string, int location, int length)
{
    bool result = false;

    BEGIN_AX_OBJC_EXCEPTIONS
    AccessibilityUIElement::s_controller->executeOnAXThreadAndWait([text = [NSString stringWithJSStringRef:string], range = NSMakeRange(location, length), this, &result] {
        result = [m_element accessibilityReplaceRange:range withText:text];
    });
    END_AX_OBJC_EXCEPTIONS

    return result;
}

bool AccessibilityUIElement::insertText(JSStringRef text)
{
    bool result = false;
    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([text = [NSString stringWithJSStringRef:text], &result, this] {
        result = [m_element accessibilityInsertText:text];
    });
    END_AX_OBJC_EXCEPTIONS
    return result;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarkerForBounds(int x, int y, int width, int height)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValueForParameter(NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute,
        [NSValue valueWithRect:NSMakeRect(x, y, width, height)]);
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForPoint(int x, int y)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValueForParameter(@"AXTextMarkerForPosition", [NSValue valueWithPoint:NSMakePoint(x, y)]);
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElement::accessibilityElementForTextMarker(AccessibilityTextMarker* marker)
{
    if (!marker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto uiElement = attributeValueForParameter(@"AXUIElementForTextMarker", marker->platformTextMarker());
    if (uiElement)
        return AccessibilityUIElement::create(uiElement.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

static JSRetainPtr<JSStringRef> createJSStringRef(id string)
{
    auto mutableString = adoptNS([[NSMutableString alloc] init]);
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

    BEGIN_AX_OBJC_EXCEPTIONS
    auto string = attributeValueForParameter(@"AXAttributedStringForTextMarkerRange", markerRange->platformTextMarkerRange());
    if ([string isKindOfClass:[NSAttributedString class]])
        return createJSStringRef(string.get());
    END_AX_OBJC_EXCEPTIONS

    return nil;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange* markerRange, bool includeSpellCheck)
{
    if (!markerRange || !markerRange->platformTextMarkerRange())
        return nullptr;

    id parameter = nil;
    if (includeSpellCheck)
        parameter = @{ @"AXSpellCheck": includeSpellCheck ? @YES : @NO, @"AXTextMarkerRange": markerRange->platformTextMarkerRange() };
    else
        parameter = markerRange->platformTextMarkerRange();

    BEGIN_AX_OBJC_EXCEPTIONS
    auto string = attributeValueForParameter(@"AXAttributedStringForTextMarkerRangeWithOptions", parameter);
    if ([string isKindOfClass:[NSAttributedString class]])
        return createJSStringRef(string.get());
    END_AX_OBJC_EXCEPTIONS

    return nil;
}

bool AccessibilityUIElement::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef attribute, AccessibilityTextMarkerRange* range)
{
    if (!range)
        return false;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto string = attributeValueForParameter(@"AXAttributedStringForTextMarkerRange", range->platformTextMarkerRange());
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
    auto indexNumber = attributeValueForParameter(@"AXIndexForTextMarker", marker->platformTextMarker());
    return [indexNumber intValue];
    END_AX_OBJC_EXCEPTIONS

    return -1;
}

bool AccessibilityUIElement::isTextMarkerValid(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return false;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValueForParameter(@"AXTextMarkerIsValid", textMarker->platformTextMarker());
    return [value boolValue];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::textMarkerForIndex(int textIndex)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValueForParameter(@"AXTextMarkerForIndex", [NSNumber numberWithInteger:textIndex]);
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::startTextMarker()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValue(@"AXStartTextMarker");
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::endTextMarker()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValue(@"AXEndTextMarker");
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::leftWordTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXLeftWordTextMarkerRangeForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::rightWordTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXRightWordTextMarkerRangeForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousWordStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto previousWordStartMarker = attributeValueForParameter(@"AXPreviousWordStartTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(previousWordStartMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextWordEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto nextWordEndMarker = attributeValueForParameter(@"AXNextWordEndTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(nextWordEndMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::paragraphTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXParagraphTextMarkerRangeForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousParagraphStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto previousParagraphStartMarker = attributeValueForParameter(@"AXPreviousParagraphStartTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(previousParagraphStartMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextParagraphEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto nextParagraphEndMarker = attributeValueForParameter(@"AXNextParagraphEndTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(nextParagraphEndMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElement::sentenceTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXSentenceTextMarkerRangeForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::previousSentenceStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto previousParagraphStartMarker = attributeValueForParameter(@"AXPreviousSentenceStartTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(previousParagraphStartMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElement::nextSentenceEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto nextParagraphEndMarker = attributeValueForParameter(@"AXNextSentenceEndTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(nextParagraphEndMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

static NSString *_convertMathMultiscriptPairsToString(NSArray *pairs)
{
    __block NSMutableString *result = [NSMutableString string];
    [pairs enumerateObjectsUsingBlock:^(id pair, NSUInteger index, BOOL *stop) {
        for (NSString *key in pair) {
            auto element = AccessibilityUIElement::create([pair objectForKey:key]);
            auto subrole = element->attributeValue(NSAccessibilitySubroleAttribute);
            [result appendFormat:@"\t%lu. %@ = %@\n", (unsigned long)index, key, subrole.get()];
        }
    }];

    return result;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPostscriptsDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto pairs = attributeValue(@"AXMathPostscripts");
    return [_convertMathMultiscriptPairsToString(pairs.get()) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::mathPrescriptsDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto pairs = attributeValue(@"AXMathPrescripts");
    return [_convertMathMultiscriptPairsToString(pairs.get()) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSValueRef AccessibilityUIElement::mathRootRadicand() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return makeJSArray(makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(@"AXMathRootRadicand").get()));
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::pathDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSMutableString *result = [NSMutableString stringWithString:@"\nStart Path\n"];
    auto bezierPath = attributeValue(NSAccessibilityPathAttribute);

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

NSArray *AccessibilityUIElement::actionNames() const
{
    NSArray *actions = nil;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([this, &actions] {
        actions = [m_element accessibilityActionNames];
    });
    END_AX_OBJC_EXCEPTIONS

    return actions;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::supportedActions() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [[actionNames() componentsJoinedByString:@","] createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

void AccessibilityUIElement::performAction(NSString *actionName) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThread([actionName, this] {
        [m_element accessibilityPerformAction:actionName];
    });
    END_AX_OBJC_EXCEPTIONS
}

bool AccessibilityUIElement::isInsertion() const
{
    return false;
}

bool AccessibilityUIElement::isDeletion() const
{
    return false;
}

bool AccessibilityUIElement::isFirstItemInSuggestion() const
{
    return false;
}

bool AccessibilityUIElement::isLastItemInSuggestion() const
{
    return false;
}

} // namespace WTR

#endif // ENABLE(ACCESSIBILITY)
