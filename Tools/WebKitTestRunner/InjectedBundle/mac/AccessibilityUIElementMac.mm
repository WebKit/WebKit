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
#import "AccessibilityUIElementMac.h"

#import "AccessibilityCommonCocoa.h"
#import "AccessibilityNotificationHandler.h"
#import "InjectedBundle.h"
#import "InjectedBundlePage.h"
#import "JSBasics.h"
#import <AppKit/NSAccessibility.h>
#import <Foundation/Foundation.h>
#import <JavaScriptCore/JSObjectRef.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <WebCore/CocoaAccessibilityConstants.h>
#import <WebCore/DateComponents.h>
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

#ifndef NSAccessibilityTextInputMarkedRangeAttribute
#define NSAccessibilityTextInputMarkedRangeAttribute @"AXTextInputMarkedRange"
#endif

#ifndef NSAccessibilityTextInputMarkedTextMarkerRangeAttribute
#define NSAccessibilityTextInputMarkedTextMarkerRangeAttribute @"AXTextInputMarkedTextMarkerRange"
#endif

#ifndef NSAccessibilityIntersectionWithSelectionRangeAttribute
#define NSAccessibilityIntersectionWithSelectionRangeAttribute @"AXIntersectionWithSelectionRange"
#endif

typedef void (*AXPostedNotificationCallback)(id element, NSString* notification, void* context);

@interface NSObject (WebKitAccessibilityAdditions)
- (BOOL)accessibilityReplaceRange:(NSRange)range withText:(NSString *)string;
- (BOOL)accessibilityInsertText:(NSString *)text;
- (NSArray *)accessibilityArrayAttributeValues:(NSString *)attribute index:(NSUInteger)index maxCount:(NSUInteger)maxCount;
- (NSArray *)_accessibilityChildrenFromIndex:(NSUInteger)index maxCount:(NSUInteger)maxCount returnPlatformElements:(BOOL)returnPlatformElements;
- (NSUInteger)accessibilityIndexOfChild:(id)child;
- (NSUInteger)accessibilityArrayAttributeCount:(NSString *)attribute;
- (void)_accessibilityScrollToMakeVisibleWithSubFocus:(NSRect)rect;
- (void)_accessibilityScrollToGlobalPoint:(NSPoint)point;
- (void)_accessibilitySetValue:(id)value forAttribute:(NSString*)attributeName;
// For site-isolation testing use only.
- (id)_accessibilityHitTest:(NSPoint)point returnPlatformElements:(BOOL)returnPlatformElements;
- (void)_accessibilityHitTestResolvingRemoteFrame:(NSPoint)point callback:(void(^)(NSString *))callback;
@end

namespace WTR {

Ref<AccessibilityUIElementMac> AccessibilityUIElementMac::create(PlatformUIElement element)
{
    return adoptRef(*new AccessibilityUIElementMac(element));
}

Ref<AccessibilityUIElementMac> AccessibilityUIElementMac::create(const AccessibilityUIElementMac& other)
{
    return adoptRef(*new AccessibilityUIElementMac(other));
}

AccessibilityUIElementMac::AccessibilityUIElementMac(id element)
    : AccessibilityUIElement(element)
    , m_element(element)
{
    if (!s_controller)
        s_controller = InjectedBundle::singleton().accessibilityController();
}

AccessibilityUIElementMac::AccessibilityUIElementMac(const AccessibilityUIElementMac& other)
    : AccessibilityUIElement(other)
    , m_element(other.m_element)
{
}

AccessibilityUIElementMac::~AccessibilityUIElementMac() = default;

bool AccessibilityUIElementMac::isEqual(AccessibilityUIElement* otherElement)
{
    if (!otherElement)
        return false;
    return platformUIElement() == static_cast<AccessibilityUIElementMac*>(otherElement)->platformUIElement();
}

RetainPtr<NSArray> supportedAttributes(id element)
{
    RetainPtr<NSMutableArray> attributes;

    BEGIN_AX_OBJC_EXCEPTIONS
    AccessibilityUIElementMac::s_controller->executeOnAXThreadAndWait([&attributes, &element] {
        attributes = [[element accessibilityAttributeNames] mutableCopy];
        // Exposing this in tests is not valuable, so remove it to decrease test maintenance burden.
        [attributes removeObject:@"AXPerformsOwnTextStitching"];
        [attributes removeObject:@"AXPostsOwnLiveRegionAnnouncements"];
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

    // These are internal APIs used by DRT/WKTR; tests are allowed to use them but we don't want to advertise them.
    static NeverDestroyed<RetainPtr<NSArray>> internalAttributes = @[
        @"AXARIAPressedIsPresent",
        @"AXARIARole",
        @"AXAutocompleteValue",
        @"AXClickPoint",
        @"AXControllerFor",
        @"AXControllers",
        @"AXDRTSpeechAttribute",
        @"AXDateTimeComponentsType",
        @"AXDescribedBy",
        @"AXDescriptionFor",
        @"AXDetailsFor",
        @"AXErrorMessageFor",
        @"AXFlowFrom",
        @"AXIsInCell",
        @"_AXIsInTable",
        @"AXIsInDescriptionListDetail",
        @"AXIsInDescriptionListTerm",
        @"AXIsIndeterminate",
        @"AXIsMultiSelectable",
        @"AXIsOnScreen",
        @"AXIsRemoteFrame",
        @"AXLabelFor",
        @"AXLabelledBy",
        @"AXLineRectsAndText",
        @"AXOwners",
        @"_AXPageRelativePosition",
        @"AXStringValue",
        @"AXValueAutofillType",

        // FIXME: these shouldn't be here, but removing one of these causes tests to fail.
        @"AXARIACurrent",
        @"AXARIALive",
        @"AXDescription",
        @"AXKeyShortcutsValue",
        @"AXOwns",
        @"AXPopupValue",
        @"AXValue",
    ];

    NSArray<NSString *> *supportedAttributes = [element accessibilityAttributeNames];
    if (![supportedAttributes containsObject:attribute] && ![internalAttributes.get() containsObject:attribute] && ![attribute isEqualToString:NSAccessibilityRoleAttribute])
        return nil;
    return [element accessibilityAttributeValue:attribute];
}

void setAttributeValue(id element, NSString* attribute, id value, bool synchronous = false)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    AccessibilityUIElementMac::s_controller->executeOnAXThreadAndWait([&element, &attribute, &value, &synchronous] {
        // FIXME: should always be asynchronous, fix tests.
        synchronous ? [element _accessibilitySetValue:value forAttribute:attribute] :
            [element accessibilitySetValue:value forAttribute:attribute];
    });
    END_AX_OBJC_EXCEPTIONS
}

RetainPtr<NSString> AccessibilityUIElementMac::descriptionOfValue(id valueObject) const
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
    [attribute getCharacters:buffer.mutableSpan().data()];
    buffer.append(':');
    buffer.append(' ');

    Vector<UniChar> valueBuffer([value length]);
    [value getCharacters:valueBuffer.mutableSpan().data()];
    buffer.appendVector(valueBuffer);

    return adopt(JSStringCreateWithCharacters(buffer.span().data(), buffer.size()));
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
            [searchStringsParameter addObject:toWTFString(context, searchStrings).createNSString().get()];
        else {
            JSObjectRef searchStringsArray = JSValueToObject(context, searchStrings, nullptr);
            unsigned searchStringsArrayLength = arrayLength(context, searchStringsArray);
            for (unsigned i = 0; i < searchStringsArrayLength; ++i)
                [searchStringsParameter addObject:toWTFString(context, JSObjectGetPropertyAtIndex(context, searchStringsArray, i, nullptr)).createNSString().get()];
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
            [searchStringsParameter addObject:toWTFString(context, searchStrings).createNSString().get()];
        else {
            JSObjectRef searchStringsArray = JSValueToObject(context, searchStrings, nullptr);
            unsigned searchStringsArrayLength = arrayLength(context, searchStringsArray);
            for (unsigned i = 0; i < searchStringsArrayLength; ++i)
                [searchStringsParameter addObject:toWTFString(context, JSObjectGetPropertyAtIndex(context, searchStringsArray, i, nullptr)).createNSString().get()];
        }
        [parameterizedAttribute setObject:searchStringsParameter forKey:@"AXSearchTextSearchStrings"];
    }

    if (startFrom)
        [parameterizedAttribute setObject:[NSString stringWithJSStringRef:startFrom] forKey:@"AXSearchTextStartFrom"];

    if (direction)
        [parameterizedAttribute setObject:[NSString stringWithJSStringRef:direction] forKey:@"AXSearchTextDirection"];

    return parameterizedAttribute;
}

static NSDictionary *textOperationParameterizedAttribute(JSContextRef context, JSStringRef operationType, JSValueRef markerRanges, JSValueRef replacementStrings, bool shouldSmartReplace)
{
    NSMutableDictionary *attributeParameters = [NSMutableDictionary dictionary];

    if (operationType)
        [attributeParameters setObject:[NSString stringWithJSStringRef:operationType] forKey:@"AXTextOperationType"];

    if (markerRanges) {
        JSObjectRef markerRangesArray = JSValueToObject(context, markerRanges, nullptr);
        unsigned markerRangesArrayLength = arrayLength(context, markerRangesArray);
        NSMutableArray *platformRanges = [NSMutableArray array];
        for (unsigned i = 0; i < markerRangesArrayLength; ++i) {
            auto propertyAtIndex = JSObjectGetPropertyAtIndex(context, markerRangesArray, i, nullptr);
            auto markerRangeRef = toTextMarkerRange(JSValueToObject(context, propertyAtIndex, nullptr));
            [platformRanges addObject:markerRangeRef->platformTextMarkerRange()];
        }
        [attributeParameters setObject:platformRanges forKey:@"AXTextOperationMarkerRanges"];
    }

    if (JSValueIsString(context, replacementStrings))
        [attributeParameters setObject:toWTFString(context, replacementStrings).createNSString().get() forKey:@"AXTextOperationReplacementString"];
    else {
        NSMutableArray *individualReplacementStringsParameter = [NSMutableArray array];
        JSObjectRef replacementStringsArray = JSValueToObject(context, replacementStrings, nullptr);
        unsigned replacementStringsArrayLength = arrayLength(context, replacementStringsArray);
        for (unsigned i = 0; i < replacementStringsArrayLength; ++i)
            [individualReplacementStringsParameter addObject:toWTFString(context, JSObjectGetPropertyAtIndex(context, replacementStringsArray, i, nullptr)).createNSString().get()];

        [attributeParameters setObject:individualReplacementStringsParameter forKey:@"AXTextOperationIndividualReplacementStrings"];
    }

    [attributeParameters setObject:[NSNumber numberWithBool:shouldSmartReplace] forKey:@"AXTextOperationSmartReplace"];

    return attributeParameters;
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

RetainPtr<id> AccessibilityUIElementMac::attributeValue(NSString *attributeName) const
{
    RetainPtr<id> value;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([this, &attributeName, &value] {
        value = WTR::attributeValue(m_element.getAutoreleased(), attributeName);
    });
    END_AX_OBJC_EXCEPTIONS

    return value;
}

RetainPtr<id> AccessibilityUIElementMac::attributeValueForParameter(NSString *attributeName, id parameter) const
{
    RetainPtr<id> value;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([this, &attributeName, &parameter, &value] {
        value = [m_element accessibilityAttributeValue:attributeName forParameter:parameter];
    });
    END_AX_OBJC_EXCEPTIONS

    return value;
}

unsigned AccessibilityUIElementMac::arrayAttributeCount(NSString *attributeName) const
{
    unsigned count = 0;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([&attributeName, &count, this] {
        count = [m_element accessibilityArrayAttributeCount:attributeName];
    });
    END_AX_OBJC_EXCEPTIONS

    return count;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::domIdentifier() const
{
    return stringAttributeValueNS(NSAccessibilityDOMIdentifierAttribute);
}

void AccessibilityUIElementMac::getLinkedUIElements(Vector<RefPtr<AccessibilityUIElement>>& elementVector)
{
    elementVector = makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(NSAccessibilityLinkedUIElementsAttribute).get());
}

void AccessibilityUIElementMac::getDocumentLinks(Vector<RefPtr<AccessibilityUIElement>>& elementVector)
{
    elementVector = makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(@"AXLinkUIElements").get());
}

void AccessibilityUIElementMac::getUIElementsWithAttribute(JSStringRef attribute, Vector<RefPtr<AccessibilityUIElement>>& elements) const
{
    auto value = attributeValue([NSString stringWithJSStringRef:attribute]);
    if ([value isKindOfClass:[NSArray class]])
        elements = makeVector<RefPtr<AccessibilityUIElement>>(value.get());
}

JSValueRef AccessibilityUIElementMac::children(JSContextRef context)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return makeJSArray(context, makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(NSAccessibilityChildrenAttribute).get()));
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

Vector<RefPtr<AccessibilityUIElement>> AccessibilityUIElementMac::getChildren() const
{
    return makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(NSAccessibilityChildrenAttribute).get());
}

Vector<RefPtr<AccessibilityUIElement>> AccessibilityUIElementMac::getChildrenInRange(unsigned location, unsigned length) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    RetainPtr<NSArray> children;
    s_controller->executeOnAXThreadAndWait([&children, location, length, this] {
        children = [m_element accessibilityArrayAttributeValues:NSAccessibilityChildrenAttribute index:location maxCount:length];
    });
    return makeVector<RefPtr<AccessibilityUIElement>>(children.get());
    END_AX_OBJC_EXCEPTIONS

    return { };
}

unsigned AccessibilityUIElementMac::childrenCount()
{
    return getChildren().size();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::childAtIndex(unsigned index)
{
    auto children = getChildrenInRange(index, 1);
    return children.size() == 1 ? children[0] : nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::childAtIndexWithRemoteElement(unsigned index)
{
    RetainPtr<NSArray> children;
    s_controller->executeOnAXThreadAndWait([&children, index, this] {
        if ([m_element respondsToSelector:@selector(_accessibilityChildrenFromIndex:maxCount:returnPlatformElements:)])
            children = [m_element _accessibilityChildrenFromIndex:index maxCount:1 returnPlatformElements:NO];
    });
    auto resultChildren = makeVector<RefPtr<AccessibilityUIElement>>(children.get());
    return resultChildren.size() == 1 ? resultChildren[0] : nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::customContent() const
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

JSValueRef AccessibilityUIElementMac::rowHeaders(JSContextRef context)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    Vector<RefPtr<AccessibilityUIElement>> elements;
    auto value = attributeValue(NSAccessibilityRowHeaderUIElementsAttribute);
    if ([value isKindOfClass:[NSArray class]])
        elements = makeVector<RefPtr<AccessibilityUIElement>>(value.get());
    return makeJSArray(context, elements);
    END_AX_OBJC_EXCEPTIONS
}

JSValueRef AccessibilityUIElementMac::selectedCells(JSContextRef context)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(NSAccessibilitySelectedCellsAttribute);
    if ([value isKindOfClass:[NSArray class]])
        return makeJSArray(context, makeVector<RefPtr<AccessibilityUIElement>>(value.get()));
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}

JSValueRef AccessibilityUIElementMac::columnHeaders(JSContextRef context)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    Vector<RefPtr<AccessibilityUIElement>> elements;
    auto value = attributeValue(NSAccessibilityColumnHeaderUIElementsAttribute);
    if ([value isKindOfClass:[NSArray class]])
        elements = makeVector<RefPtr<AccessibilityUIElement>>(value.get());
    return makeJSArray(context, elements);
    END_AX_OBJC_EXCEPTIONS
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::elementAtPoint(int x, int y)
{
    RetainPtr<id> element;
    s_controller->executeOnAXThreadAndWait([&x, &y, &element, this] {
        element = [m_element accessibilityHitTest:NSMakePoint(x, y)];
    });

    if (!element)
        return nullptr;

    return AccessibilityUIElementMac::create(element.get());
}

RefPtr<AccessibilityUIElement>  AccessibilityUIElementMac::elementAtPointWithRemoteElement(int x, int y)
{
    RetainPtr<id> element;
    s_controller->executeOnAXThreadAndWait([&x, &y, &element, this] {
        element = [m_element _accessibilityHitTest:NSMakePoint(x, y) returnPlatformElements:NO];
    });

    if (!element)
        return nullptr;

    return AccessibilityUIElementMac::create(element.get());
}

void AccessibilityUIElementMac::elementAtPointResolvingRemoteFrame(JSContextRef context, int x, int y, JSValueRef jsCallback)
{
    JSValueProtect(context, jsCallback);
    s_controller->executeOnAXThreadAndWait([x, y, protectedThis = Ref { *this }, jsCallback = WTF::move(jsCallback), context = JSRetainPtr { JSContextGetGlobalContext(context) }] () mutable {
        auto callback = [jsCallback = WTF::move(jsCallback), context = WTF::move(context)](NSString *result) mutable {
            s_controller->executeOnMainThread([result = WTF::move(result), jsCallback = WTF::move(jsCallback), context = WTF::move(context)] () {
                JSValueRef arguments[1];
                arguments[0] = makeValueRefForValue(context.get(), result);
                JSObjectCallAsFunction(context.get(), const_cast<JSObjectRef>(jsCallback), 0, 1, arguments, 0);
                JSValueUnprotect(context.get(), jsCallback);
            });
        };

        [protectedThis->m_element _accessibilityHitTestResolvingRemoteFrame:NSMakePoint(x, y) callback:WTF::move(callback)];
    });
}

unsigned AccessibilityUIElementMac::indexOfChild(AccessibilityUIElement* element)
{
    unsigned index;
    id platformElement = static_cast<AccessibilityUIElementMac*>(element)->platformUIElement();
    s_controller->executeOnAXThreadAndWait([&platformElement, &index, this] {
        index = [m_element accessibilityIndexOfChild:platformElement];
    });
    return index;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::elementForAttribute(NSString *attribute) const
{
    auto element = attributeValue(attribute);
    return element ? AccessibilityUIElementMac::create(element.get()) : RefPtr<AccessibilityUIElement>();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::elementForAttributeAtIndex(NSString* attribute, unsigned index) const
{
    auto elements = attributeValue(attribute);
    return index < [elements count] ? AccessibilityUIElementMac::create([elements objectAtIndex:index]) : RefPtr<AccessibilityUIElement>();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::linkedUIElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(NSAccessibilityLinkedUIElementsAttribute, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::controllerElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXControllers", index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::ariaControlsElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXControllerFor", index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::ariaDescribedByElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXDescribedBy", index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::descriptionForElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXDescriptionFor", index);
}

JSValueRef AccessibilityUIElementMac::detailsElements(JSContextRef context)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto elements = attributeValue(@"AXDetailsElements");
    if ([elements isKindOfClass:NSArray.class])
        return makeJSArray(context, makeVector<RefPtr<AccessibilityUIElement>>(elements.get()));
    END_AX_OBJC_EXCEPTIONS
    return { };
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::ariaDetailsElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXDetailsElements", index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::detailsForElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXDetailsFor", index);
}

JSValueRef AccessibilityUIElementMac::errorMessageElements(JSContextRef context)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto elements = attributeValue(@"AXErrorMessageElements");
    if ([elements isKindOfClass:NSArray.class])
        return makeJSArray(context, makeVector<RefPtr<AccessibilityUIElement>>(elements.get()));
    END_AX_OBJC_EXCEPTIONS
    return { };
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::ariaErrorMessageElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXErrorMessageElements", index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::errorMessageForElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXErrorMessageFor", index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::flowFromElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXFlowFrom", index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::ariaFlowToElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(NSAccessibilityLinkedUIElementsAttribute, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::ariaLabelledByElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXLabelledBy", index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::labelForElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXLabelFor", index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::ownerElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(@"AXOwners", index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::ariaOwnsElementAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(NSAccessibilityOwnsAttribute, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::disclosedRowAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(NSAccessibilityDisclosedRowsAttribute, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::rowAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(NSAccessibilityRowsAttribute, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::activeElement() const
{
    return elementForAttribute(@"AXActiveElement");
}

JSValueRef AccessibilityUIElementMac::selectedChildren(JSContextRef context)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto children = attributeValue(NSAccessibilitySelectedChildrenAttribute);
    if ([children isKindOfClass:NSArray.class])
        return makeJSArray(context, makeVector<RefPtr<AccessibilityUIElement>>(children.get()));
    END_AX_OBJC_EXCEPTIONS
    return makeJSArray(context, { });
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::selectedChildAtIndex(unsigned index) const
{
    return elementForAttributeAtIndex(NSAccessibilitySelectedChildrenAttribute, index);
}

unsigned AccessibilityUIElementMac::selectedChildrenCount() const
{
    return arrayAttributeCount(NSAccessibilitySelectedChildrenAttribute);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::selectedRowAtIndex(unsigned index)
{
    return elementForAttributeAtIndex(NSAccessibilitySelectedRowsAttribute, index);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::titleUIElement()
{
    return elementForAttribute(NSAccessibilityTitleUIElementAttribute);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::parentElement()
{
    return elementForAttribute(NSAccessibilityParentAttribute);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::disclosedByRow()
{
    return elementForAttribute(NSAccessibilityDisclosedByRowAttribute);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::attributesOfLinkedUIElements()
{
    Vector<RefPtr<AccessibilityUIElement> > linkedElements;
    getLinkedUIElements(linkedElements);
    return descriptionOfElements(linkedElements);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::attributesOfDocumentLinks()
{
    Vector<RefPtr<AccessibilityUIElement> > linkElements;
    getDocumentLinks(linkElements);
    return descriptionOfElements(linkElements);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::attributesOfChildren()
{
    return descriptionOfElements(getChildren());
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::allAttributes()
{
    auto attributes = supportedAttributes(m_element.getAutoreleased());

    NSMutableString *values = [NSMutableString string];
    for (NSString *attribute in attributes.get()) {
        // Exclude screen-specific values, since they can change depending on the system.
        if ([attribute isEqualToString:@"AXPosition"]
            || [attribute isEqualToString:@"_AXPrimaryScreenHeight"]
            || [attribute isEqualToString:@"AXRelativeFrame"])
            continue;

        if ([attribute isEqualToString:@"AXVisibleCharacterRange"]) {
            RetainPtr value = attributeValue(NSAccessibilityRoleAttribute);
            RetainPtr<NSString> role = [value isKindOfClass:[NSString class]] ? (NSString *)value.get() : nil;
            if (role.get() == nil || [role isEqualToString:@"AXList"] || [role isEqualToString:@"AXLink"] || [role isEqualToString:@"AXGroup"] || [role isEqualToString:@"AXRow"] || [role isEqualToString:@"AXColumn"] || [role isEqualToString:@"AXTable"] || [role isEqualToString:@"AXWebArea"]) {
                // For some roles, behavior with ITM on and ITM off differ for this API in ways
                // that are not clearly meaningful to any actual user-facing behavior. Skip dumping this
                // attribute for all of the "dump every attribute for every element" tests.
                // We can test visible-character-range in dedicated tests.
                continue;
            }
        }

        auto value = descriptionOfValue(attributeValue(attribute).get());

        if (!value
            && ([attribute isEqualToString:NSAccessibilityTextInputMarkedRangeAttribute]
                || [attribute isEqualToString:NSAccessibilityTextInputMarkedTextMarkerRangeAttribute]))
            continue;

        [values appendFormat:@"%@: %@\n", attribute, value.get()];
    }

    return [values createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::stringDescriptionOfAttributeValue(JSStringRef attribute)
{
    auto value = attributeValue([NSString stringWithJSStringRef:attribute]);
    auto valueDescription = descriptionOfValue(value.get());
    return [valueDescription createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::stringAttributeValue(JSStringRef attribute)
{
    return stringAttributeValueNS([NSString stringWithJSStringRef:attribute]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::stringAttributeValueNS(NSString *attribute) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(attribute);
    if ([value isKindOfClass:[NSString class]])
        return [value createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

double AccessibilityUIElementMac::numberAttributeValue(JSStringRef attribute)
{
    return numberAttributeValueNS([NSString stringWithJSStringRef:attribute]);
}

double AccessibilityUIElementMac::numberAttributeValueNS(NSString *attribute) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(attribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [value doubleValue];
    END_AX_OBJC_EXCEPTIONS

    return 0;
}

JSValueRef AccessibilityUIElementMac::uiElementArrayAttributeValue(JSContextRef context, JSStringRef attribute)
{
    Vector<RefPtr<AccessibilityUIElement>> elements;
    getUIElementsWithAttribute(attribute, elements);
    return makeJSArray(context, elements);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::uiElementAttributeValue(JSStringRef attribute) const
{
    if (auto value = attributeValue([NSString stringWithJSStringRef:attribute]))
        return AccessibilityUIElementMac::create(value.get());
    return nullptr;
}

bool AccessibilityUIElementMac::boolAttributeValueNS(NSString *attribute) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(attribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

bool AccessibilityUIElementMac::boolAttributeValue(JSStringRef attribute)
{
    return boolAttributeValueNS([NSString stringWithJSStringRef:attribute]);
}

void AccessibilityUIElementMac::attributeValueAsync(JSContextRef context, JSStringRef attribute, JSValueRef callback)
{
    if (!attribute || !callback)
        return;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([attribute = retainPtr([NSString stringWithJSStringRef:attribute]), callback = WTF::move(callback), context = JSRetainPtr { JSContextGetGlobalContext(context) }, this] () mutable {
        id value = [m_element accessibilityAttributeValue:attribute.get()];
        if ([value isKindOfClass:[NSArray class]] || [value isKindOfClass:[NSDictionary class]])
            value = [value description];

        s_controller->executeOnMainThread([value = retainPtr(value), callback = WTF::move(callback), context = WTF::move(context)] () {
            JSValueRef arguments[1];
            arguments[0] = makeValueRefForValue(context.get(), value.get());
            JSObjectCallAsFunction(context.get(), const_cast<JSObjectRef>(callback), 0, 1, arguments, 0);
        });
    });
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElementMac::setBoolAttributeValue(JSStringRef attribute, bool value)
{
    setAttributeValue(m_element.getAutoreleased(), [NSString stringWithJSStringRef:attribute], @(value), /* synchronous */ true);
}

void AccessibilityUIElementMac::setValue(JSStringRef value)
{
    setAttributeValue(m_element.getAutoreleased(), NSAccessibilityValueAttribute, [NSString stringWithJSStringRef:value]);
}

bool AccessibilityUIElementMac::isAttributeSettable(JSStringRef attribute)
{
    return isAttributeSettableNS([NSString stringWithJSStringRef:attribute]);
}

bool AccessibilityUIElementMac::isAttributeSettableNS(NSString *attribute) const
{
    bool value = false;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([this, &attribute, &value] {
        value = [m_element accessibilityIsAttributeSettable:attribute];
    });
    END_AX_OBJC_EXCEPTIONS

    return value;
}

bool AccessibilityUIElementMac::isAttributeSupported(JSStringRef attribute)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [supportedAttributes(m_element.getAutoreleased()) containsObject:[NSString stringWithJSStringRef:attribute]];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::parameterizedAttributeNames()
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

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::role()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto role = descriptionOfValue(attributeValue(NSAccessibilityRoleAttribute).get());
    return concatenateAttributeAndValue(@"AXRole", role.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::subrole()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto subrole = descriptionOfValue(attributeValue(NSAccessibilitySubroleAttribute).get());
    return concatenateAttributeAndValue(@"AXSubrole", subrole.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::roleDescription()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto role = descriptionOfValue(attributeValue(NSAccessibilityRoleDescriptionAttribute).get());
    return concatenateAttributeAndValue(@"AXRoleDescription", role.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::computedRoleString()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto computedRoleString = descriptionOfValue(attributeValue(@"AXARIARole").get());
    return [computedRoleString createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::title()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto title = descriptionOfValue(attributeValue(NSAccessibilityTitleAttribute).get());
    return concatenateAttributeAndValue(@"AXTitle", title.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::description()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto description = descriptionOfValue(attributeValue(NSAccessibilityDescriptionAttribute).get());
    return concatenateAttributeAndValue(@"AXDescription", description.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::brailleLabel() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto description = descriptionOfValue(attributeValue(@"AXBrailleLabel").get());
    return concatenateAttributeAndValue(@"AXBrailleLabel", description.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::brailleRoleDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto description = descriptionOfValue(attributeValue(@"AXBrailleRoleDescription").get());
    return concatenateAttributeAndValue(@"AXBrailleRoleDescription", description.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::liveRegionStatus() const
{
    return stringAttributeValueNS(@"AXARIALive");
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::liveRegionRelevant() const
{
    return stringAttributeValueNS(@"AXARIARelevant");
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::orientation() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto description = descriptionOfValue(attributeValue(NSAccessibilityOrientationAttribute).get());
    return concatenateAttributeAndValue(@"AXOrientation", description.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::stringValue()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    RetainPtr<id> value;
    auto role = attributeValue(NSAccessibilityRoleAttribute);
    if ([role isEqualToString:@"AXDateTimeArea"])
        value = attributeValue(@"AXStringValue");
    else
        value = attributeValue(NSAccessibilityValueAttribute);

    if (auto description = descriptionOfValue(value.get()))
        return concatenateAttributeAndValue(@"AXValue", description.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::dateValue()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(NSAccessibilityValueAttribute);
    if (![value isKindOfClass:[NSDate class]])
        return nullptr;

    // Adjust the returned date per time zone and daylight savings in an equivalent way to what VoiceOver does.
    NSInteger offset = [[NSTimeZone localTimeZone] secondsFromGMTForDate:[NSDate date]];
    auto type = attributeValue(@"AXDateTimeComponentsType");
    if ([type unsignedShortValue] != (uint8_t)WebCore::DateComponentsType::DateTimeLocal && [[NSTimeZone localTimeZone] isDaylightSavingTimeForDate:[NSDate date]])
        offset -= 3600;
    value = [NSDate dateWithTimeInterval:offset sinceDate:value.get()];
    if (auto description = descriptionOfValue(value.get()))
        return concatenateAttributeAndValue(@"AXDateValue", description.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::dateTimeValue() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return stringAttributeValueNS(@"AXDateTimeValue");
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::language()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto description = descriptionOfValue(attributeValue(@"AXLanguage").get());
    return concatenateAttributeAndValue(@"AXLanguage", description.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::helpText() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto description = descriptionOfValue(attributeValue(NSAccessibilityHelpAttribute).get());
    return concatenateAttributeAndValue(@"AXHelp", description.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

double AccessibilityUIElementMac::pageX()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto positionValue = attributeValue(@"_AXPageRelativePosition");
    return static_cast<double>([positionValue pointValue].x);
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElementMac::pageY()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto positionValue = attributeValue(@"_AXPageRelativePosition");
    return static_cast<double>([positionValue pointValue].y);
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElementMac::x()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto positionValue = attributeValue(NSAccessibilityPositionAttribute);
    return static_cast<double>([positionValue pointValue].x);
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElementMac::y()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto positionValue = attributeValue(NSAccessibilityPositionAttribute);
    return static_cast<double>([positionValue pointValue].y);
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElementMac::width()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto sizeValue = attributeValue(NSAccessibilitySizeAttribute);
    return static_cast<double>([sizeValue sizeValue].width);
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElementMac::height()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto sizeValue = attributeValue(NSAccessibilitySizeAttribute);
    return static_cast<double>([sizeValue sizeValue].height);
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElementMac::clickPointX()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto positionValue = attributeValue(@"AXClickPoint");
    return static_cast<double>([positionValue pointValue].x);
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

double AccessibilityUIElementMac::clickPointY()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto positionValue = attributeValue(@"AXClickPoint");
    return static_cast<double>([positionValue pointValue].y);
    END_AX_OBJC_EXCEPTIONS

    return 0.0f;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::lineRectsAndText() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto lineRectsAndText = attributeValue(@"AXLineRectsAndText");
    if (![lineRectsAndText isKindOfClass:NSArray.class])
        return { };
    return [[lineRectsAndText componentsJoinedByString:@"|"] createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return { };
}

double AccessibilityUIElementMac::intValue() const
{
    return numberAttributeValueNS(NSAccessibilityValueAttribute);
}

double AccessibilityUIElementMac::minValue()
{
    return numberAttributeValueNS(NSAccessibilityMinValueAttribute);
}

double AccessibilityUIElementMac::maxValue()
{
    return numberAttributeValueNS(NSAccessibilityMaxValueAttribute);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::valueDescription()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto valueDescription = attributeValue(NSAccessibilityValueDescriptionAttribute);
    if ([valueDescription isKindOfClass:[NSString class]])
        return concatenateAttributeAndValue(@"AXValueDescription", valueDescription.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

unsigned AccessibilityUIElementMac::numberOfCharacters() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(NSAccessibilityNumberOfCharactersAttribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber *)value unsignedIntValue];
    END_AX_OBJC_EXCEPTIONS
    return 0;
}

int AccessibilityUIElementMac::insertionPointLineNumber()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(NSAccessibilityInsertionPointLineNumberAttribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber *)value intValue];
    END_AX_OBJC_EXCEPTIONS

    return -1;
}

bool AccessibilityUIElementMac::isPressActionSupported()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [actionNames() containsObject:NSAccessibilityPressAction];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

bool AccessibilityUIElementMac::isIncrementActionSupported()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [actionNames() containsObject:NSAccessibilityIncrementAction];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

bool AccessibilityUIElementMac::isDecrementActionSupported()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [actionNames() containsObject:NSAccessibilityDecrementAction];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

bool AccessibilityUIElementMac::isAtomicLiveRegion() const
{
    return boolAttributeValueNS(@"AXARIAAtomic");
}

bool AccessibilityUIElementMac::isBusy() const
{
    return boolAttributeValueNS(@"AXElementBusy");
}

bool AccessibilityUIElementMac::isEnabled()
{
    return boolAttributeValueNS(NSAccessibilityEnabledAttribute);
}

bool AccessibilityUIElementMac::isRequired() const
{
    return boolAttributeValueNS(@"AXRequired");
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::focusedElement() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    if (auto focus = attributeValue(NSAccessibilityFocusedUIElementAttribute))
        return AccessibilityUIElementMac::create(focus.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

bool AccessibilityUIElementMac::isFocused() const
{
    return boolAttributeValueNS(NSAccessibilityFocusedAttribute);
}

bool AccessibilityUIElementMac::isSelected() const
{
    auto value = attributeValue(NSAccessibilitySelectedAttribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    return false;
}

bool AccessibilityUIElementMac::isSelectedOptionActive() const
{
    return false;
}

bool AccessibilityUIElementMac::isIndeterminate() const
{
    return boolAttributeValueNS(@"AXIsIndeterminate");
}

bool AccessibilityUIElementMac::isValid() const
{
    return m_element.getAutoreleased();
}

bool AccessibilityUIElementMac::isExpanded() const
{
    return boolAttributeValueNS(NSAccessibilityExpandedAttribute);
}

bool AccessibilityUIElementMac::isChecked() const
{
    // On the Mac, intValue()==1 if a a checkable control is checked.
    return intValue() == 1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::currentStateValue() const
{
    return stringAttributeValueNS(NSAccessibilityARIACurrentAttribute);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::sortDirection() const
{
    return stringAttributeValueNS(NSAccessibilitySortDirectionAttribute);
}

int AccessibilityUIElementMac::hierarchicalLevel() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(NSAccessibilityDisclosureLevelAttribute);
    if ([value isKindOfClass:[NSNumber class]])
        return [value intValue];
    END_AX_OBJC_EXCEPTIONS

    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::classList() const
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

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::speakAs()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(@"AXDRTSpeechAttribute");
    if ([value isKindOfClass:[NSArray class]])
        return [[(NSArray *)value componentsJoinedByString:@", "] createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

bool AccessibilityUIElementMac::isGrabbed() const
{
    return boolAttributeValueNS(NSAccessibilityGrabbedAttribute);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::ariaDropEffects() const
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
int AccessibilityUIElementMac::lineForIndex(int index)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValueForParameter(NSAccessibilityLineForIndexParameterizedAttribute, @(index));
    if ([value isKindOfClass:[NSNumber class]])
        return [(NSNumber *)value intValue];
    END_AX_OBJC_EXCEPTIONS

    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::rangeForLine(int line)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValueForParameter(NSAccessibilityRangeForLineParameterizedAttribute, @(line));
    if ([value isKindOfClass:[NSValue class]])
        return [NSStringFromRange([value rangeValue]) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::rangeForPosition(int x, int y)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValueForParameter(NSAccessibilityRangeForPositionParameterizedAttribute, [NSValue valueWithPoint:NSMakePoint(x, y)]);
    if ([value isKindOfClass:[NSValue class]])
        return [NSStringFromRange([value rangeValue]) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

static NSMutableString* makeBoundsDescription(NSRect rect, bool exposePosition)
{
    return [NSMutableString stringWithFormat:@"{{%f, %f}, {%f, %f}}", exposePosition ? rect.origin.x : -1.0f, exposePosition ? rect.origin.y : -1.0f, rect.size.width, rect.size.height];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::boundsForRange(unsigned location, unsigned length)
{
    NSRange range = NSMakeRange(location, length);
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValueForParameter(NSAccessibilityBoundsForRangeParameterizedAttribute, [NSValue valueWithRange:range]);
    NSRect rect = NSMakeRect(0, 0, 0, 0);
    if ([value isKindOfClass:[NSValue class]])
        rect = [value rectValue];

    // don't return position information because it is platform dependent
    NSMutableString* boundsDescription = makeBoundsDescription(rect, false /* exposePosition */);
    return [boundsDescription createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::boundsForRangeWithPagePosition(unsigned location, unsigned length)
{
    NSRange range = NSMakeRange(location, length);
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValueForParameter(@"_AXPageBoundsForTextMarkerRange", [NSValue valueWithRange:range]);
    NSRect rect = NSMakeRect(0, 0, 0, 0);
    if ([value isKindOfClass:[NSValue class]])
        rect = [value rectValue];

    NSMutableString* boundsDescription = makeBoundsDescription(rect, true /* exposePosition */);
    return [boundsDescription createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::stringForRange(unsigned location, unsigned length)
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

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::attributedStringForRange(unsigned location, unsigned length)
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

bool AccessibilityUIElementMac::attributedStringRangeIsMisspelled(unsigned location, unsigned length)
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

unsigned AccessibilityUIElementMac::uiElementCountForSearchPredicate(JSContextRef context, AccessibilityUIElement *startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameter = searchPredicateForSearchCriteria(context, startElement, nullptr, isDirectionNext, UINT_MAX, searchKey, searchText, visibleOnly, immediateDescendantsOnly);
    auto value = attributeValueForParameter(@"AXUIElementsForSearchPredicate", parameter);
    if ([value isKindOfClass:[NSArray class]])
        return [value count];
    END_AX_OBJC_EXCEPTIONS

    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement *startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameter = searchPredicateForSearchCriteria(context, startElement, nullptr, isDirectionNext, 1, searchKey, searchText, visibleOnly, immediateDescendantsOnly);
    auto searchResults = attributeValueForParameter(@"AXUIElementsForSearchPredicate", parameter);
    if (![searchResults isKindOfClass:[NSArray class]] || ![searchResults count])
        return nullptr;

    id result = [searchResults firstObject];
    if ([result isKindOfClass:NSDictionary.class]) {
        RELEASE_ASSERT([result objectForKey:@"AXSearchResultElement"]);
        return AccessibilityUIElementMac::create([result objectForKey:@"AXSearchResultElement"]);
    }
    return AccessibilityUIElementMac::create(result);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::selectTextWithCriteria(JSContextRef context, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
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
JSValueRef AccessibilityUIElementMac::searchTextWithCriteria(JSContextRef context, JSValueRef searchStrings, JSStringRef startFrom, JSStringRef direction)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameterizedAttribute = searchTextParameterizedAttributeForCriteria(context, searchStrings, startFrom, direction);
    auto result = attributeValueForParameter(@"AXSearchTextWithCriteria", parameterizedAttribute);
    if ([result isKindOfClass:[NSArray class]])
        return makeJSArray(context, makeVector<RefPtr<AccessibilityTextMarkerRange>>(result.get()));
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSValueRef AccessibilityUIElementMac::performTextOperation(JSContextRef context, JSStringRef operationType, JSValueRef markerRanges, JSValueRef replacementStrings, bool shouldSmartReplace)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameterizedAttribute = textOperationParameterizedAttribute(context, operationType, markerRanges, replacementStrings, shouldSmartReplace);
    auto result = attributeValueForParameter(@"AXTextOperation", parameterizedAttribute);
    if ([result isKindOfClass:[NSArray class]])
        return makeValueRefForValue(context, result.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}
#endif

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::attributesOfColumnHeaders()
{
    // not yet defined in AppKit... odd
    BEGIN_AX_OBJC_EXCEPTIONS
    auto columnHeaders = attributeValue(@"AXColumnHeaderUIElements");
    auto columnHeadersVector = makeVector<RefPtr<AccessibilityUIElement>>(columnHeaders.get());
    return descriptionOfElements(columnHeadersVector);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::attributesOfRowHeaders()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto rowHeaders = attributeValue(@"AXRowHeaderUIElements");
    auto rowHeadersVector = makeVector<RefPtr<AccessibilityUIElement>>(rowHeaders.get());
    return descriptionOfElements(rowHeadersVector);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::attributesOfColumns()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto columns = attributeValue(NSAccessibilityColumnsAttribute);
    auto columnsVector = makeVector<RefPtr<AccessibilityUIElement>>(columns.get());
    return descriptionOfElements(columnsVector);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSValueRef AccessibilityUIElementMac::columns(JSContextRef context)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return makeJSArray(context, makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(NSAccessibilityColumnsAttribute).get()));
    END_AX_OBJC_EXCEPTIONS
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::attributesOfRows()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto rows = attributeValue(NSAccessibilityRowsAttribute);
    auto rowsVector = makeVector<RefPtr<AccessibilityUIElement>>(rows.get());
    return descriptionOfElements(rowsVector);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::attributesOfVisibleCells()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto cells = attributeValue(@"AXVisibleCells");
    auto cellsVector = makeVector<RefPtr<AccessibilityUIElement>>(cells.get());
    return descriptionOfElements(cellsVector);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::attributesOfHeader()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto header = attributeValue(NSAccessibilityHeaderAttribute);
    if (!header)
        return [@"" createJSStringRef];

    Vector<RefPtr<AccessibilityUIElement>> headerVector;
    headerVector.append(AccessibilityUIElementMac::create(header.get()));
    return descriptionOfElements(headerVector);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

int AccessibilityUIElementMac::rowCount()
{
    return arrayAttributeCount(NSAccessibilityRowsAttribute);
}

int AccessibilityUIElementMac::columnCount()
{
    return arrayAttributeCount(NSAccessibilityColumnsAttribute);
}

int AccessibilityUIElementMac::indexInTable()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto indexNumber = attributeValue(NSAccessibilityIndexAttribute);
    if (indexNumber)
        return [indexNumber intValue];
    END_AX_OBJC_EXCEPTIONS

    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::rowIndexRange()
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

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::columnIndexRange()
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

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::cellForColumnAndRow(unsigned col, unsigned row)
{
    NSArray *colRowArray = @[@(col), @(row)];
    BEGIN_AX_OBJC_EXCEPTIONS
    if (auto cell = attributeValueForParameter(@"AXCellForColumnAndRow", colRowArray))
        return AccessibilityUIElementMac::create(cell.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::horizontalScrollbar() const
{
    if (!m_element)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    if (id scrollbar = attributeValue(NSAccessibilityHorizontalScrollBarAttribute).unsafeGet())
        return AccessibilityUIElementMac::create(scrollbar);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::verticalScrollbar() const
{
    if (!m_element)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    if (id scrollbar = attributeValue(NSAccessibilityVerticalScrollBarAttribute).unsafeGet())
        return AccessibilityUIElementMac::create(scrollbar);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

void AccessibilityUIElementMac::scrollToMakeVisible()
{
    performAction(@"AXScrollToVisible");
}

void AccessibilityUIElementMac::scrollToGlobalPoint(int x, int y)
{
    NSPoint point = NSMakePoint(x, y);
    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThread([point, this] {
        [m_element _accessibilityScrollToGlobalPoint:point];
    });
    END_AX_OBJC_EXCEPTIONS
}

void AccessibilityUIElementMac::scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height)
{
    NSRect rect = NSMakeRect(x, y, width, height);
    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThread([rect, this] {
        [m_element _accessibilityScrollToMakeVisibleWithSubFocus:rect];
    });
    END_AX_OBJC_EXCEPTIONS
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::selectedText()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto string = attributeValue(@"AXSelectedText");
    if (![string isKindOfClass:[NSString class]])
        return nullptr;
    return [string createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::selectedTextRange()
{
    NSRange range = NSMakeRange(NSNotFound, 0);
    BEGIN_AX_OBJC_EXCEPTIONS
    auto indexRange = attributeValue(NSAccessibilitySelectedTextRangeAttribute);
    if (indexRange)
        range = [indexRange rangeValue];
    NSString *rangeDescription = [NSString stringWithFormat:@"{%lu, %lu}", static_cast<unsigned long>(range.location), static_cast<unsigned long>(range.length)];
    return [rangeDescription createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::intersectionWithSelectionRange()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    if (auto rangeAttribute = attributeValue(NSAccessibilityIntersectionWithSelectionRangeAttribute)) {
        NSRange range = [rangeAttribute rangeValue];
        NSString *rangeDescription = [NSString stringWithFormat:@"{%lu, %lu}", static_cast<unsigned long>(range.location), static_cast<unsigned long>(range.length)];
        return [rangeDescription createJSStringRef];
    }
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

bool AccessibilityUIElementMac::setSelectedTextRange(unsigned location, unsigned length)
{
    NSRange textRange = NSMakeRange(location, length);
    NSValue *textRangeValue = [NSValue valueWithRange:textRange];
    setAttributeValue(m_element.getAutoreleased(), NSAccessibilitySelectedTextRangeAttribute, textRangeValue);

    return true;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::textInputMarkedRange() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(NSAccessibilityTextInputMarkedRangeAttribute);
    if (value)
        return [NSStringFromRange([value rangeValue]) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}

bool AccessibilityUIElementMac::dismiss()
{
    return performAction(@"AXDismissAction");
}

bool AccessibilityUIElementMac::setSelectedTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    if (!markerRange)
        return false;

    setAttributeValue(m_element.getAutoreleased(), NSAccessibilitySelectedTextMarkerRangeAttribute, markerRange->platformTextMarkerRange());

    return true;
}

void AccessibilityUIElementMac::increment()
{
    performAction(@"AXSyncIncrementAction");
}

void AccessibilityUIElementMac::decrement()
{
    performAction(@"AXSyncDecrementAction");
}

void AccessibilityUIElementMac::asyncIncrement()
{
    performAction(NSAccessibilityIncrementAction);
}

void AccessibilityUIElementMac::asyncDecrement()
{
    performAction(NSAccessibilityDecrementAction);
}

void AccessibilityUIElementMac::showMenu()
{
    performAction(NSAccessibilityShowMenuAction);
}

void AccessibilityUIElementMac::press()
{
    performAction(NSAccessibilityPressAction);
}

void AccessibilityUIElementMac::syncPress()
{
    performAction(@"AXSyncPressAction");
}

void AccessibilityUIElementMac::setSelectedChild(AccessibilityUIElement* element) const
{
    NSArray* array = element ? @[static_cast<AccessibilityUIElementMac*>(element)->platformUIElement()] : @[];
    setAttributeValue(m_element.getAutoreleased(), NSAccessibilitySelectedChildrenAttribute, array);
}

void AccessibilityUIElementMac::setSelectedChildAtIndex(unsigned index) const
{
    RefPtr<AccessibilityUIElement> element = const_cast<AccessibilityUIElementMac*>(this)->childAtIndex(index);
    if (!element)
        return;

    auto selectedChildren = attributeValue(NSAccessibilitySelectedChildrenAttribute);
    NSArray *array = @[static_cast<AccessibilityUIElementMac*>(element.get())->platformUIElement()];
    if (selectedChildren)
        array = [selectedChildren arrayByAddingObjectsFromArray:array];

    setAttributeValue(m_element.getAutoreleased(), NSAccessibilitySelectedChildrenAttribute, array, /* synchronous */ true);
}

void AccessibilityUIElementMac::removeSelectionAtIndex(unsigned index) const
{
    RefPtr<AccessibilityUIElement> element = const_cast<AccessibilityUIElementMac*>(this)->childAtIndex(index);
    if (!element)
        return;

    auto selectedChildren = attributeValue(NSAccessibilitySelectedChildrenAttribute);
    if (!selectedChildren)
        return;

    NSMutableArray *array = [NSMutableArray arrayWithArray:selectedChildren.get()];
    [array removeObject:static_cast<AccessibilityUIElementMac*>(element.get())->platformUIElement()];

    setAttributeValue(m_element.getAutoreleased(), NSAccessibilitySelectedChildrenAttribute, array, /* synchronous */ true);
}

void AccessibilityUIElementMac::clearSelectedChildren() const
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::accessibilityValue() const
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::url()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto url = attributeValue(NSAccessibilityURLAttribute);
    if ([url isKindOfClass:[NSURL class]])
        return [[url absoluteString] createJSStringRef];
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}

bool AccessibilityUIElementMac::addNotificationListener(JSContextRef context, JSValueRef functionCallback)
{
    if (!functionCallback)
        return false;

    // Mac programmers should not be adding more than one notification listener per element.
    // Other platforms may be different.
    if (m_notificationHandler)
        return false;

    m_notificationHandler = adoptNS([[AccessibilityNotificationHandler alloc] initWithContext:context]);
    [m_notificationHandler setPlatformElement:platformUIElement()];
    [m_notificationHandler setCallback:functionCallback];
    [m_notificationHandler startObserving];

    return true;
}

bool AccessibilityUIElementMac::removeNotificationListener()
{
    // Mac programmers should not be trying to remove a listener that's already removed.
    ASSERT(m_notificationHandler);

    [m_notificationHandler stopObserving];
    m_notificationHandler = nil;

    return true;
}

bool AccessibilityUIElementMac::isFocusable() const
{
    return isAttributeSettableNS(NSAccessibilityFocusedAttribute);
}

bool AccessibilityUIElementMac::isSelectable() const
{
    return isAttributeSettableNS(NSAccessibilitySelectedAttribute);
}

bool AccessibilityUIElementMac::isMultiSelectable() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(@"AXIsMultiSelectable");
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    END_AX_OBJC_EXCEPTIONS
    return false;
}

bool AccessibilityUIElementMac::isVisible() const
{
    return false;
}

bool AccessibilityUIElementMac::isOnScreen() const
{
    auto value = attributeValue(@"AXIsOnScreen");
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    return false;
}

bool AccessibilityUIElementMac::isOffScreen() const
{
    return false;
}

bool AccessibilityUIElementMac::isCollapsed() const
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::embeddedImageDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = descriptionOfValue(attributeValue(@"AXEmbeddedImageDescription").get());
    return concatenateAttributeAndValue(@"AXEmbeddedImageDescription", value.get());
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}

JSValueRef AccessibilityUIElementMac::imageOverlayElements(JSContextRef context)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return makeJSArray(context, makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(@"AXImageOverlayElements").get()));
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

bool AccessibilityUIElementMac::isIgnored() const
{
    BOOL result = NO;
    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([&result, this] {
        result = m_element ? [m_element accessibilityIsIgnored] : YES;
    });
    END_AX_OBJC_EXCEPTIONS
    return result;
}

bool AccessibilityUIElementMac::isSingleLine() const
{
    return false;
}

bool AccessibilityUIElementMac::isMultiLine() const
{
    return false;
}

bool AccessibilityUIElementMac::hasPopup() const
{
    return boolAttributeValueNS(@"AXHasPopup");
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::popupValue() const
{
    if (auto result = stringAttributeValueNS(@"AXPopupValue"))
        return result;

    return [@"false" createJSStringRef];
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::focusableAncestor()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    if (id ancestor = attributeValue(@"AXFocusableAncestor").unsafeGet())
        return AccessibilityUIElementMac::create(ancestor);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::editableAncestor()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    if (id ancestor = attributeValue(@"AXEditableAncestor").unsafeGet())
        return AccessibilityUIElementMac::create(ancestor);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::highestEditableAncestor()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    if (id ancestor = attributeValue(@"AXHighestEditableAncestor").unsafeGet())
        return AccessibilityUIElementMac::create(ancestor);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

bool AccessibilityUIElementMac::isInDescriptionListDetail() const
{
    return boolAttributeValueNS(@"AXIsInDescriptionListDetail");
}

bool AccessibilityUIElementMac::isInDescriptionListTerm() const
{
    return boolAttributeValueNS(@"AXIsInDescriptionListTerm");
}

bool AccessibilityUIElementMac::isInCell() const
{
    return boolAttributeValueNS(@"AXIsInCell");
}

bool AccessibilityUIElementMac::isInTable() const
{
    return boolAttributeValueNS(@"_AXIsInTable");
}

void AccessibilityUIElementMac::takeFocus()
{
    setAttributeValue(m_element.getAutoreleased(), NSAccessibilityFocusedAttribute, @YES);
}

void AccessibilityUIElementMac::takeSelection()
{
}

void AccessibilityUIElementMac::addSelection()
{
}

void AccessibilityUIElementMac::removeSelection()
{
}

// Text markers
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXLineTextMarkerRangeForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::rightLineTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXRightLineTextMarkerRangeForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::leftLineTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXLeftLineTextMarkerRangeForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::previousLineStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto marker = attributeValueForParameter(@"AXPreviousLineStartTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(marker.get());
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::nextLineEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto marker = attributeValueForParameter(@"AXNextLineEndTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(marker.get());
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}

int AccessibilityUIElementMac::lineIndexForTextMarker(AccessibilityTextMarker* marker) const
{
    if (!marker)
        return -1;

    BEGIN_AX_OBJC_EXCEPTIONS
    return [attributeValueForParameter(@"AXLineForTextMarker", marker->platformTextMarker()) intValue];
    END_AX_OBJC_EXCEPTIONS

    return -1;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::styleTextMarkerRangeForTextMarker(AccessibilityTextMarker* marker)
{
    if (!marker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXStyleTextMarkerRangeForTextMarker", marker->platformTextMarker());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::textMarkerRangeForSearchPredicate(JSContextRef context, AccessibilityTextMarkerRange *startRange, bool forward, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameter = searchPredicateForSearchCriteria(context, nullptr, startRange, forward, 1, searchKey, searchText, visibleOnly, immediateDescendantsOnly);
    auto searchResults = attributeValueForParameter(@"AXRangesForSearchPredicate", parameter);
    if (![searchResults isKindOfClass:[NSArray class]] || ![searchResults count])
        return nullptr;

    id result = [searchResults firstObject];
    if (![result isKindOfClass:NSDictionary.class])
        return nullptr;

    id rangeRef = [result objectForKey:@"AXSearchResultRange"];
    if (rangeRef && CFGetTypeID((__bridge CFTypeRef)rangeRef) == AXTextMarkerRangeGetTypeID())
        return AccessibilityTextMarkerRange::create(rangeRef);
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::misspellingTextMarkerRange(AccessibilityTextMarkerRange* start, bool forward)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    NSDictionary *parameters = misspellingSearchParameterizedAttributeForCriteria(start, forward);
    auto textMarkerRange = attributeValueForParameter(@"AXMisspellingTextMarkerRange", parameters);
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::textMarkerRangeForElement(AccessibilityUIElement* element)
{
    if (!element)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXTextMarkerRangeForUIElement", static_cast<AccessibilityUIElementMac*>(element)->platformUIElement());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

int AccessibilityUIElementMac::textMarkerRangeLength(AccessibilityTextMarkerRange* range)
{
    if (!range)
        return 0;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto lengthValue = attributeValueForParameter(@"AXLengthForTextMarkerRange", range->platformTextMarkerRange());
    return [lengthValue intValue];
    END_AX_OBJC_EXCEPTIONS

    return 0;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::previousTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto previousMarker = attributeValueForParameter(@"AXPreviousTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(previousMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::nextTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto nextMarker = attributeValueForParameter(@"AXNextTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(nextMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::textMarkerRangeForLine(long lineIndex)
{
    if (lineIndex < 0)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXTextMarkerRangeForLine", @(static_cast<unsigned>(lineIndex)));
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::stringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    if (!markerRange)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textString = attributeValueForParameter(@"AXStringForTextMarkerRange", markerRange->platformTextMarkerRange());
    return [textString createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::rectsForTextMarkerRange(AccessibilityTextMarkerRange*, JSStringRef)
{
    // Not implemented on macOS
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    if (!startMarker->platformTextMarker() || !endMarker->platformTextMarker())
        return nullptr;
    NSArray *textMarkers = @[startMarker->platformTextMarker(), endMarker->platformTextMarker()];
    auto textMarkerRange = attributeValueForParameter(@"AXTextMarkerRangeForTextMarkers", textMarkers);
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::textMarkerRangeForUnorderedMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    if (!startMarker->platformTextMarker() || !endMarker->platformTextMarker())
        return nullptr;
    NSArray *textMarkers = @[startMarker->platformTextMarker(), endMarker->platformTextMarker()];
    auto textMarkerRange = attributeValueForParameter(@"AXTextMarkerRangeForUnorderedTextMarkers", textMarkers);
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::textMarkerRangeForRange(unsigned location, unsigned length)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return AccessibilityTextMarkerRange::create(attributeValueForParameter(@"_AXTextMarkerRangeForNSRange",
        [NSValue valueWithRange:NSMakeRange(location, length)]).get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::selectedTextMarkerRange()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValue(NSAccessibilitySelectedTextMarkerRangeAttribute);
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

void AccessibilityUIElementMac::resetSelectedTextMarkerRange()
{
    auto start = attributeValue(@"AXStartTextMarker");
    if (!start)
        return;

    NSArray *textMarkers = @[start.get(), start.get()];
    auto textMarkerRange = attributeValueForParameter(@"AXTextMarkerRangeForUnorderedTextMarkers", textMarkers);
    if (!textMarkerRange)
        return;

    setAttributeValue(m_element.getAutoreleased(), NSAccessibilitySelectedTextMarkerRangeAttribute, textMarkerRange.get(), /* synchronous */ true);
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::textInputMarkedTextMarkerRange() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValue(NSAccessibilityTextInputMarkedTextMarkerRangeAttribute);
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    if (!range)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValueForParameter(@"_AXStartTextMarkerForTextMarkerRange", range->platformTextMarkerRange());
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    if (!range)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValueForParameter(@"_AXEndTextMarkerForTextMarkerRange", range->platformTextMarkerRange());
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::endTextMarkerForBounds(int x, int y, int width, int height)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValueForParameter(NSAccessibilityEndTextMarkerForBoundsParameterizedAttribute,
        [NSValue valueWithRect:NSMakeRect(x, y, width, height)]);
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

bool AccessibilityUIElementMac::replaceTextInRange(JSStringRef string, int location, int length)
{
    bool result = false;

    BEGIN_AX_OBJC_EXCEPTIONS
    AccessibilityUIElementMac::s_controller->executeOnAXThreadAndWait([text = [NSString stringWithJSStringRef:string], range = NSMakeRange(location, length), this, &result] {
        result = [m_element accessibilityReplaceRange:range withText:text];
    });
    END_AX_OBJC_EXCEPTIONS

    return result;
}

bool AccessibilityUIElementMac::insertText(JSStringRef text)
{
    bool result = false;
    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([text = [NSString stringWithJSStringRef:text], &result, this] {
        result = [m_element accessibilityInsertText:text];
    });
    END_AX_OBJC_EXCEPTIONS
    return result;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::startTextMarkerForBounds(int x, int y, int width, int height)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValueForParameter(NSAccessibilityStartTextMarkerForBoundsParameterizedAttribute,
        [NSValue valueWithRect:NSMakeRect(x, y, width, height)]);
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::textMarkerForPoint(int x, int y)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValueForParameter(@"AXTextMarkerForPosition", [NSValue valueWithPoint:NSMakePoint(x, y)]);
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementMac::accessibilityElementForTextMarker(AccessibilityTextMarker* marker)
{
    if (!marker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto uiElement = attributeValueForParameter(@"AXUIElementForTextMarker", marker->platformTextMarker());
    if (uiElement)
        return AccessibilityUIElementMac::create(uiElement.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

static NSString *descriptionForColor(CGColorRef color)
{
    // This is a hack to get an OK description for a CGColor by crudely parsing its debug description, e.g.:
    //
    // <CGColor 0x13bf07670> <CGColorSpace 0x12be0ce40> [ (kCGColorSpaceICCBased; kCGColorSpaceModelRGB; sRGB IEC61966-2.1)] ( 0 0 0 0 )
    // Ideally we convert it to a WebCore::Color and then call serializationForCSS(const Color&), but I can't
    // get that to link succesfully, despite these symbols being WEBCORE_EXPORTed.
    auto string = adoptNS([[NSMutableString alloc] init]);
    [string appendFormat:@"%@", color];
    NSArray *stringComponents = [string componentsSeparatedByString:@">"];
    if (stringComponents.count) {
        NSString *bracketsRemovedString = [[stringComponents objectAtIndex:stringComponents.count - 1] stringByReplacingOccurrencesOfString:@"]" withString:@""];
        return [bracketsRemovedString stringByReplacingOccurrencesOfString:@"headroom = 1.000000 " withString:@""];
    }
    return nil;
}

static void appendColorDescription(RetainPtr<NSMutableString> string, NSString* attributeKey, NSDictionary<NSString *, id> *attributes)
{
    id color = [attributes objectForKey:attributeKey];
    if (!color)
        return;

    if (CFGetTypeID(color) == CGColorGetTypeID())
        [string appendFormat:@"%@:%@\n", attributeKey, descriptionForColor((CGColorRef)color)];
}

static JSRetainPtr<JSStringRef> createJSStringRef(id string, bool includeDidSpellCheck)
{
    auto mutableString = adoptNS([[NSMutableString alloc] init]);
    id attributeEnumerationBlock = ^(NSDictionary<NSString *, id> *attributes, NSRange range, BOOL *stop) {
        [mutableString appendFormat:@"Attributes in range %@:\n", NSStringFromRange(range)];
        BOOL misspelled = [[attributes objectForKey:NSAccessibilityMisspelledTextAttribute] boolValue];
        if (misspelled)
            misspelled = [[attributes objectForKey:NSAccessibilityMarkedMisspelledTextAttribute] boolValue];
        if (misspelled)
            [mutableString appendString:@"Misspelled, "];
        if (includeDidSpellCheck) {
            BOOL didSpellCheck = [[attributes objectForKey:@"AXDidSpellCheck"] boolValue];
            [mutableString appendFormat:@"AXDidSpellCheck: %d\n", didSpellCheck];
        }
        id font = [attributes objectForKey:(__bridge id)kAXFontTextAttribute];
        if (font)
            [mutableString appendFormat:@"%@: %@\n", (__bridge id)kAXFontTextAttribute, font];

        appendColorDescription(mutableString, NSAccessibilityForegroundColorTextAttribute, attributes);
        appendColorDescription(mutableString, NSAccessibilityBackgroundColorTextAttribute, attributes);

        int scriptState = [[attributes objectForKey:NSAccessibilitySuperscriptTextAttribute] intValue];
        if (scriptState == -1) {
            // -1 == subscript
            [mutableString appendFormat:@"%@: -1\n", NSAccessibilitySuperscriptTextAttribute];
        } else if (scriptState == 1) {
            // 1 == superscript
            [mutableString appendFormat:@"%@: 1\n", NSAccessibilitySuperscriptTextAttribute];
        }

        BOOL hasTextShadow = [[attributes objectForKey:NSAccessibilityShadowTextAttribute] boolValue];
        if (hasTextShadow)
            [mutableString appendFormat:@"%@: YES\n", NSAccessibilityShadowTextAttribute];

        BOOL hasUnderline = [[attributes objectForKey:NSAccessibilityUnderlineTextAttribute] boolValue];
        if (hasUnderline) {
            [mutableString appendFormat:@"%@: YES\n", NSAccessibilityUnderlineTextAttribute];
            appendColorDescription(mutableString, NSAccessibilityUnderlineColorTextAttribute, attributes);
        }

        BOOL hasLinethrough = [[attributes objectForKey:NSAccessibilityStrikethroughTextAttribute] boolValue];
        if (hasLinethrough) {
            [mutableString appendFormat:@"%@: YES\n", NSAccessibilityStrikethroughTextAttribute];
            appendColorDescription(mutableString, NSAccessibilityStrikethroughColorTextAttribute, attributes);
        }

        id attachment = [attributes objectForKey:NSAccessibilityAttachmentTextAttribute];
        if (attachment)
            [mutableString appendFormat:@"%@: {present}\n", NSAccessibilityAttachmentTextAttribute];

        if ([attributes objectForKey:NSAccessibilityTableAttribute])
            [mutableString appendFormat:@"%@: {present}\n", NSAccessibilityTableAttribute];
    };
    [string enumerateAttributesInRange:NSMakeRange(0, [string length]) options:(NSAttributedStringEnumerationOptions)0 usingBlock:attributeEnumerationBlock];
    [mutableString appendString:[string string]];
    return [mutableString createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    if (!markerRange)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto string = attributeValueForParameter(@"AXAttributedStringForTextMarkerRange", markerRange->platformTextMarkerRange());
    if ([string isKindOfClass:[NSAttributedString class]])
        return createJSStringRef(string.get(), /* IncludeDidSpellCheck */ false);
    END_AX_OBJC_EXCEPTIONS

    return nil;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::attributedStringForTextMarkerRangeWithDidSpellCheck(AccessibilityTextMarkerRange* markerRange)
{
    if (!markerRange)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto string = attributeValueForParameter(@"AXAttributedStringForTextMarkerRange", markerRange->platformTextMarkerRange());
    if ([string isKindOfClass:[NSAttributedString class]])
        return createJSStringRef(string.get(), /* IncludeDidSpellCheck */ true);
    END_AX_OBJC_EXCEPTIONS

    return nil;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange* markerRange, bool includeSpellCheck)
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
        return createJSStringRef(string.get(), /* IncludeDidSpellCheck */ false);
    END_AX_OBJC_EXCEPTIONS

    return nil;
}

bool AccessibilityUIElementMac::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef attribute, AccessibilityTextMarkerRange* range)
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

int AccessibilityUIElementMac::indexForTextMarker(AccessibilityTextMarker* marker)
{
    if (!marker)
        return -1;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto indexNumber = attributeValueForParameter(@"AXIndexForTextMarker", marker->platformTextMarker());
    return [indexNumber intValue];
    END_AX_OBJC_EXCEPTIONS

    return -1;
}

bool AccessibilityUIElementMac::isTextMarkerNull(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return true;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValueForParameter(@"AXTextMarkerIsNull", textMarker->platformTextMarker());
    return [value boolValue];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

bool AccessibilityUIElementMac::isTextMarkerValid(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return false;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValueForParameter(@"AXTextMarkerIsValid", textMarker->platformTextMarker());
    return [value boolValue];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

bool AccessibilityUIElementMac::isTextMarkerRangeValid(AccessibilityTextMarkerRange* textMarkerRange)
{
    if (!textMarkerRange)
        return false;

    BEGIN_AX_OBJC_EXCEPTIONS
    return [[m_element accessibilityAttributeValue:@"AXTextMarkerRangeIsValid" forParameter:textMarkerRange->platformTextMarkerRange()] boolValue];
    END_AX_OBJC_EXCEPTIONS

    return false;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::textMarkerForIndex(int textIndex)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValueForParameter(@"AXTextMarkerForIndex", [NSNumber numberWithInteger:textIndex]);
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::startTextMarker()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValue(@"AXStartTextMarker");
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::endTextMarker()
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarker = attributeValue(@"AXEndTextMarker");
    return AccessibilityTextMarker::create(textMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::leftWordTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXLeftWordTextMarkerRangeForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::rightWordTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXRightWordTextMarkerRangeForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::previousWordStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto previousWordStartMarker = attributeValueForParameter(@"AXPreviousWordStartTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(previousWordStartMarker.get());
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::nextWordEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto nextWordEndMarker = attributeValueForParameter(@"AXNextWordEndTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(nextWordEndMarker.get());
    END_AX_OBJC_EXCEPTIONS
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::paragraphTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXParagraphTextMarkerRangeForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::previousParagraphStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto previousParagraphStartMarker = attributeValueForParameter(@"AXPreviousParagraphStartTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(previousParagraphStartMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::nextParagraphEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto nextParagraphEndMarker = attributeValueForParameter(@"AXNextParagraphEndTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(nextParagraphEndMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementMac::sentenceTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto textMarkerRange = attributeValueForParameter(@"AXSentenceTextMarkerRangeForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarkerRange::create(textMarkerRange.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::previousSentenceStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto previousParagraphStartMarker = attributeValueForParameter(@"AXPreviousSentenceStartTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(previousParagraphStartMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementMac::nextSentenceEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    if (!textMarker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    auto nextParagraphEndMarker = attributeValueForParameter(@"AXNextSentenceEndTextMarkerForTextMarker", textMarker->platformTextMarker());
    return AccessibilityTextMarker::create(nextParagraphEndMarker.get());
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::textMarkerDebugDescription(AccessibilityTextMarker* marker)
{
    if (!marker)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    RetainPtr description = attributeValueForParameter(@"AXTextMarkerDebugDescription", marker->platformTextMarker());
    return [description createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::textMarkerRangeDebugDescription(AccessibilityTextMarkerRange* range)
{
    if (!range)
        return nullptr;

    BEGIN_AX_OBJC_EXCEPTIONS
    RetainPtr description = attributeValueForParameter(@"AXTextMarkerRangeDebugDescription", range->platformTextMarkerRange());
    return [description createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

static NSString *_convertMathMultiscriptPairsToString(NSArray *pairs)
{
    __block NSMutableString *result = [NSMutableString string];
    [pairs enumerateObjectsUsingBlock:^(id pair, NSUInteger index, BOOL *stop) {
        for (NSString *key in pair) {
            auto element = AccessibilityUIElementMac::create([pair objectForKey:key]);
            auto subrole = element->attributeValue(NSAccessibilitySubroleAttribute);
            [result appendFormat:@"\t%lu. %@ = %@\n", (unsigned long)index, key, subrole.get()];
        }
    }];

    return result;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::mathPostscriptsDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto pairs = attributeValue(@"AXMathPostscripts");
    return [_convertMathMultiscriptPairsToString(pairs.get()) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::mathPrescriptsDescription() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto pairs = attributeValue(@"AXMathPrescripts");
    return [_convertMathMultiscriptPairsToString(pairs.get()) createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSValueRef AccessibilityUIElementMac::mathRootRadicand(JSContextRef context)
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return makeJSArray(context, makeVector<RefPtr<AccessibilityUIElement>>(attributeValue(@"AXMathRootRadicand").get()));
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::pathDescription() const
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
        default:
            break;
        }
    }

    return [result createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

NSArray *AccessibilityUIElementMac::actionNames() const
{
    NSArray *actions = nil;

    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThreadAndWait([this, &actions] {
        actions = [m_element accessibilityActionNames];
    });
    END_AX_OBJC_EXCEPTIONS

    return actions;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementMac::supportedActions() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    return [[actionNames() componentsJoinedByString:@","] createJSStringRef];
    END_AX_OBJC_EXCEPTIONS

    return nullptr;
}

bool AccessibilityUIElementMac::performAction(NSString *actionName) const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    s_controller->executeOnAXThread([actionName, this] {
        [m_element accessibilityPerformAction:actionName];
    });
    END_AX_OBJC_EXCEPTIONS

    // macOS actions don't return a value.
    return true;
}

bool AccessibilityUIElementMac::isInsertion() const
{
    return false;
}

bool AccessibilityUIElementMac::isDeletion() const
{
    return false;
}

bool AccessibilityUIElementMac::isFirstItemInSuggestion() const
{
    return false;
}

bool AccessibilityUIElementMac::isLastItemInSuggestion() const
{
    return false;
}

bool AccessibilityUIElementMac::isRemoteFrame() const
{
    BEGIN_AX_OBJC_EXCEPTIONS
    auto value = attributeValue(@"AXIsRemoteFrame");
    if ([value isKindOfClass:[NSNumber class]])
        return [value boolValue];
    END_AX_OBJC_EXCEPTIONS
    return false;
}

} // namespace WTR
