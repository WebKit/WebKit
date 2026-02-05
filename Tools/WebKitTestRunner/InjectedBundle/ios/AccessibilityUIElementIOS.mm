/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "AccessibilityUIElementIOS.h"

#import "AccessibilityCommonCocoa.h"
#import "AccessibilityNotificationHandler.h"
#import "InjectedBundle.h"
#import "InjectedBundlePage.h"
#import <Foundation/Foundation.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <UIKit/UIKit.h>
#import <WebCore/TextGranularity.h>
#import <WebKit/WKBundleFrame.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/cocoa/VectorCocoa.h>

#if HAVE(ACCESSIBILITY_FRAMEWORK)
#import <Accessibility/Accessibility.h>
#endif

typedef void (*AXPostedNotificationCallback)(id element, NSString* notification, void* context);

@interface NSObject (UIAccessibilityHidden)
- (NSString *)accessibilityBrailleLabel;
- (NSString *)accessibilityBrailleRoleDescription;
- (id)accessibilityFocusedUIElement;
- (id)accessibilityHitTest:(CGPoint)point;
- (NSString *)accessibilityLanguage;
- (id)accessibilityLinkedElement;
- (id)accessibilityTitleElement;
- (NSRange)accessibilityColumnRange;
- (NSRange)accessibilityRowRange;
- (id)accessibilityElementForRow:(NSInteger)row andColumn:(NSInteger)column;
- (NSURL *)accessibilityURL;
- (NSArray *)accessibilityHeaderElements;
- (NSString *)accessibilityDatetimeValue;
- (NSArray *)accessibilityDetailsElements;
- (NSArray *)accessibilityErrorMessageElements;
- (NSString *)accessibilityPlaceholderValue;
- (NSString *)stringForRange:(NSRange)range;
- (NSAttributedString *)attributedStringForRange:(NSRange)range;
- (NSAttributedString *)attributedStringForElement;
- (NSString *)selectionRangeString;
- (NSArray *)lineRectsAndText;
- (CGPoint)accessibilityClickPoint;
- (void)accessibilityModifySelection:(WebCore::TextGranularity)granularity increase:(BOOL)increase;
- (NSDictionary<NSString *, id> *)_accessibilityResolvedEditingStyles;
- (NSRange)_accessibilitySelectedTextRange;
- (void)_accessibilitySetSelectedTextRange:(NSRange)range;
- (BOOL)accessibilityReplaceRange:(NSRange)range withText:(NSString *)string;
- (BOOL)accessibilityInsertText:(NSString *)text;
- (void)accessibilitySetPostedNotificationCallback:(AXPostedNotificationCallback)function withContext:(void*)context;
- (CGFloat)_accessibilityMinValue;
- (CGFloat)_accessibilityMaxValue;
- (void)_accessibilitySetValue:(NSString *)value;
- (void)_accessibilitySetFocus:(BOOL)focus;
- (BOOL)_accessibilityIsFocusedForTesting;
- (BOOL)_accessibilityIsSwitch;
- (void)_accessibilityActivate;
- (UIAccessibilityTraits)_axSelectedTrait;
- (UIAccessibilityTraits)_axTextAreaTrait;
- (UIAccessibilityTraits)_axSearchFieldTrait;
- (UIAccessibilityTraits)_axVisitedTrait;
- (NSString *)accessibilityCurrentState;
- (NSUInteger)accessibilityRowCount;
- (NSUInteger)accessibilityColumnCount;
- (NSUInteger)accessibilityARIARowCount;
- (NSUInteger)accessibilityARIAColumnCount;
- (NSUInteger)accessibilityARIARowIndex;
- (NSUInteger)accessibilityARIAColumnIndex;
- (NSString *)accessibilityRowIndexDescription;
- (NSString *)accessibilityColumnIndexDescription;
- (BOOL)accessibilityARIAIsBusy;
- (BOOL)accessibilityARIALiveRegionIsAtomic;
- (NSString *)accessibilityARIALiveRegionStatus;
- (NSString *)accessibilityARIARelevantStatus;
- (NSString *)accessibilityInvalidStatus;
- (UIAccessibilityTraits)_axTextEntryTrait;
- (UIAccessibilityTraits)_axTabBarTrait;
- (UIAccessibilityTraits)_axMenuItemTrait;
- (id)_accessibilityFieldsetAncestor;
- (BOOL)_accessibilityHasTouchEventListener;
- (NSString *)accessibilityExpandedTextValue;
- (NSString *)accessibilitySortDirection;
- (BOOL)accessibilityIsExpanded;
- (BOOL)accessibilitySupportsARIAExpanded;
- (BOOL)accessibilityIsIndeterminate;
- (NSUInteger)accessibilityBlockquoteLevel;
- (NSArray *)accessibilityFindMatchingObjects:(NSDictionary *)parameters;
- (NSArray<NSString *> *)accessibilitySpeechHint;
- (BOOL)_accessibilityIsStrongPasswordField;
- (CGRect)accessibilityVisibleContentRect;
- (NSString *)accessibilityTextualContext;
- (NSString *)accessibilityRoleDescription;
- (BOOL)accessibilityHasPopup;
- (NSString *)accessibilityPopupValue;
- (NSString *)accessibilityColorStringValue;
- (BOOL)accessibilityIsInDescriptionListDefinition;
- (BOOL)accessibilityIsInDescriptionListTerm;
- (BOOL)_accessibilityIsInTableCell;
- (BOOL)accessibilityIsAttributeSettable:(NSString *)attributeName;
- (BOOL)accessibilityIsRequired;
- (id)_accessibilityTableAncestor;
- (id)_accessibilityLandmarkAncestor;
- (id)_accessibilityListAncestor;
- (id)_accessibilityPhotoDescription;
- (NSArray *)accessibilityImageOverlayElements;
- (NSRange)accessibilityVisibleCharacterRange;
- (NSString *)_accessibilityWebRoleAsString;
- (BOOL)accessibilityIsDeletion;
- (BOOL)accessibilityIsInsertion;
- (BOOL)accessibilityIsFirstItemInSuggestion;
- (BOOL)accessibilityIsLastItemInSuggestion;
- (BOOL)accessibilityIsMarkAnnotation;

// TextMarker related
- (NSArray *)textMarkerRange;
- (NSInteger)lengthForTextMarkers:(NSArray *)textMarkers;
- (NSString *)stringForTextMarkers:(NSArray *)markers;
- (NSArray *)textRectsFromMarkers:(NSArray *)markers withText:(NSString *)text;
- (id)startOrEndTextMarkerForTextMarkers:(NSArray*)textMarkers isStart:(BOOL)isStart;
- (NSArray *)textMarkerRangeForMarkers:(NSArray *)textMarkers;
- (NSInteger)positionForTextMarker:(id)marker;
- (id)nextMarkerForMarker:(id)marker;
- (id)previousMarkerForMarker:(id)marker;
- (id)accessibilityObjectForTextMarker:(id)marker;
- (id)lineStartMarkerForMarker:(id)marker;
- (id)lineEndMarkerForMarker:(id)marker;
- (NSArray *)misspellingTextMarkerRange:(NSArray *)startTextMarkerRange forward:(BOOL)forward;
- (NSArray *)textMarkerRangeFromMarkers:(NSArray *)markers withText:(NSString *)text;
- (NSAttributedString *)_attributedStringForTextMarkerRangeForTesting:(NSArray *)markers;
@end

@interface NSObject (WebAccessibilityObjectWrapperPrivate)
- (NSString *)accessibilityDOMIdentifier;
- (CGPathRef)_accessibilityPath;
- (CGPoint)_accessibilityPageRelativeLocation;
@end

namespace WTR {

static JSRetainPtr<JSStringRef> concatenateAttributeAndValue(NSString *attribute, NSString *value)
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

// Factory method
Ref<AccessibilityUIElementIOS> AccessibilityUIElementIOS::create(PlatformUIElement element)
{
    return adoptRef(*new AccessibilityUIElementIOS(element));
}

Ref<AccessibilityUIElementIOS> AccessibilityUIElementIOS::create(const AccessibilityUIElementIOS& other)
{
    return adoptRef(*new AccessibilityUIElementIOS(other));
}

AccessibilityUIElementIOS::AccessibilityUIElementIOS(PlatformUIElement element)
    : AccessibilityUIElement(element)
    , m_element(element)
{
}

AccessibilityUIElementIOS::AccessibilityUIElementIOS(const AccessibilityUIElementIOS& other)
    : AccessibilityUIElement(other)
    , m_element(other.m_element)
{
}

AccessibilityUIElementIOS::~AccessibilityUIElementIOS() = default;

bool AccessibilityUIElementIOS::isValid() const
{
    return m_element.getAutoreleased();
}

bool AccessibilityUIElementIOS::isEqual(AccessibilityUIElement* otherElement)
{
    if (!otherElement)
        return false;
    return platformUIElement() == otherElement->platformUIElement();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::domIdentifier() const
{
    id value = [m_element accessibilityDOMIdentifier];
    if ([value isKindOfClass:[NSString class]])
        return [value createJSStringRef];
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::headerElementAtIndex(unsigned index)
{
    NSArray *headers = [m_element accessibilityHeaderElements];
    if (index < [headers count])
        return AccessibilityUIElement::create([headers objectAtIndex:index]);

    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::linkedElement()
{
    id linkedElement = [m_element accessibilityLinkedElement];
    if (linkedElement)
        return AccessibilityUIElement::create(linkedElement);

    return nullptr;
}

void AccessibilityUIElementIOS::getLinkedUIElements(Vector<RefPtr<AccessibilityUIElement> >& elementVector)
{
}

void AccessibilityUIElementIOS::getDocumentLinks(Vector<RefPtr<AccessibilityUIElement> >& elementVector)
{
}

JSValueRef AccessibilityUIElementIOS::children(JSContextRef context)
{
    return makeJSArray(context, makeVector<RefPtr<AccessibilityUIElement>>([m_element accessibilityElements]));
}

Vector<RefPtr<AccessibilityUIElement>> AccessibilityUIElementIOS::getChildren() const
{
    return getChildrenInRange(0, [m_element accessibilityElementCount]);
}

Vector<RefPtr<AccessibilityUIElement>> AccessibilityUIElementIOS::getChildrenInRange(unsigned location, unsigned length) const
{
    Vector<RefPtr<AccessibilityUIElement>> children;
    NSUInteger childCount = [m_element accessibilityElementCount];
    for (NSUInteger k = location; k < childCount && k < (location + length); ++k) {
        if (id child = [m_element accessibilityElementAtIndex:k])
            children.append(AccessibilityUIElement::create(child));
    }
    return children;
}

unsigned AccessibilityUIElementIOS::childrenCount()
{
    return getChildren().size();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::childAtIndex(unsigned index)
{
    auto children = getChildrenInRange(index, 1);
    return children.size() == 1 ? children[0] : nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::elementAtPoint(int x, int y)
{
    id element = [m_element accessibilityHitTest:CGPointMake(x, y)];
    if (!element)
        return nil;

    return AccessibilityUIElement::create(element);
}

unsigned AccessibilityUIElementIOS::indexOfChild(AccessibilityUIElement* element)
{
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::linkedUIElementAtIndex(unsigned index)
{
    return nullptr;
}

JSValueRef AccessibilityUIElementIOS::detailsElements(JSContextRef context)
{
    NSArray *elements = [m_element accessibilityDetailsElements];
    if ([elements isKindOfClass:NSArray.class])
        return makeJSArray(context, makeVector<RefPtr<AccessibilityUIElement>>(elements));
    return { };
}

JSValueRef AccessibilityUIElementIOS::errorMessageElements(JSContextRef context)
{
    NSArray *elements = [m_element accessibilityErrorMessageElements];
    if ([elements isKindOfClass:NSArray.class])
        return makeJSArray(context, makeVector<RefPtr<AccessibilityUIElement>>(elements));
    return { };
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::ariaOwnsElementAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::ariaFlowToElementAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::ariaControlsElementAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::ariaDetailsElementAtIndex(unsigned index)
{
    return nil;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::ariaErrorMessageElementAtIndex(unsigned index)
{
    NSArray *elements = [m_element accessibilityErrorMessageElements];
    if (![elements isKindOfClass:NSArray.class])
        return nullptr;

    if (index < elements.count)
        return create([elements objectAtIndex:index]);
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::disclosedRowAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::rowAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::selectedChildAtIndex(unsigned index) const
{
    return nullptr;
}

unsigned AccessibilityUIElementIOS::selectedChildrenCount() const
{
    return 0;
}

JSValueRef AccessibilityUIElementIOS::selectedChildren(JSContextRef context)
{
    return makeJSArray(context, { });
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::selectedRowAtIndex(unsigned index)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::titleUIElement()
{
    id titleElement = [m_element accessibilityTitleElement];
    if (titleElement)
        return AccessibilityUIElement::create(titleElement);
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::parentElement()
{
    return nil;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::disclosedByRow()
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributesOfLinkedUIElements()
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributesOfDocumentLinks()
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributesOfChildren()
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::allAttributes()
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::stringDescriptionOfAttributeValue(JSStringRef attribute)
{
    if (JSStringIsEqualToUTF8CString(attribute, "AXVisibleCharacterRange"))
        return [NSStringFromRange([m_element accessibilityVisibleCharacterRange]) createJSStringRef];

    if (JSStringIsEqualToUTF8CString(attribute, "AXResolvedEditingStyles"))
        return [[[m_element _accessibilityResolvedEditingStyles] description] createJSStringRef];

    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::stringAttributeValue(JSStringRef attribute)
{
    if (JSStringIsEqualToUTF8CString(attribute, "AXPlaceholderValue"))
        return [[m_element accessibilityPlaceholderValue] createJSStringRef];

    if (JSStringIsEqualToUTF8CString(attribute, "AXRoleDescription"))
        return [[m_element accessibilityRoleDescription] createJSStringRef];

    if (JSStringIsEqualToUTF8CString(attribute, "AXColorStringDescription"))
        return [[m_element accessibilityColorStringValue] createJSStringRef];

    if (JSStringIsEqualToUTF8CString(attribute, "AXARIACurrent"))
        return [[m_element accessibilityCurrentState] createJSStringRef];

    if (JSStringIsEqualToUTF8CString(attribute, "AXExpandedTextValue"))
        return [[m_element accessibilityExpandedTextValue] createJSStringRef];

    if (JSStringIsEqualToUTF8CString(attribute, "AXInvalid"))
        return [[m_element accessibilityInvalidStatus] createJSStringRef];

    if (JSStringIsEqualToUTF8CString(attribute, "AXSortDirection"))
        return [[m_element accessibilitySortDirection] createJSStringRef];

    if (JSStringIsEqualToUTF8CString(attribute, "AXVisibleContentRect")) {
        CGRect screenRect = [m_element accessibilityVisibleContentRect];
        NSString *rectStr = [NSString stringWithFormat:@"{%.2f, %.2f, %.2f, %.2f}", screenRect.origin.x, screenRect.origin.y, screenRect.size.width, screenRect.size.height];
        return [rectStr createJSStringRef];
    }

    if (JSStringIsEqualToUTF8CString(attribute, "AXTextualContext"))
        return [[m_element accessibilityTextualContext] createJSStringRef];

    if (JSStringIsEqualToUTF8CString(attribute, "AXRowIndexDescription"))
        return [[m_element accessibilityRowIndexDescription] createJSStringRef];

    if (JSStringIsEqualToUTF8CString(attribute, "AXColumnIndexDescription"))
        return [[m_element accessibilityColumnIndexDescription] createJSStringRef];

    return createJSString();
}

double AccessibilityUIElementIOS::numberAttributeValue(JSStringRef attribute)
{
    // Support test for table related attributes.
    if (JSStringIsEqualToUTF8CString(attribute, "AXARIAColumnCount"))
        return [m_element accessibilityARIAColumnCount];
    if (JSStringIsEqualToUTF8CString(attribute, "AXARIARowCount"))
        return [m_element accessibilityARIARowCount];
    if (JSStringIsEqualToUTF8CString(attribute, "AXARIAColumnIndex"))
        return [m_element accessibilityARIAColumnIndex];
    if (JSStringIsEqualToUTF8CString(attribute, "AXARIARowIndex"))
        return [m_element accessibilityARIARowIndex];
    if (JSStringIsEqualToUTF8CString(attribute, "AXBlockquoteLevel"))
        return [m_element accessibilityBlockquoteLevel];

    return 0;
}

JSValueRef AccessibilityUIElementIOS::uiElementArrayAttributeValue(JSContextRef, JSStringRef attribute)
{
    return nullptr;
}

JSValueRef AccessibilityUIElementIOS::rowHeaders(JSContextRef)
{
    return nullptr;
}

JSValueRef AccessibilityUIElementIOS::selectedCells(JSContextRef)
{
    return nullptr;
}

JSValueRef AccessibilityUIElementIOS::columnHeaders(JSContextRef)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::uiElementAttributeValue(JSStringRef attribute) const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::customContent() const
{
#if HAVE(ACCESSIBILITY_FRAMEWORK)
    auto customContent = adoptNS([[NSMutableArray alloc] init]);
    for (AXCustomContent *content in [m_element accessibilityCustomContent])
        [customContent addObject:[NSString stringWithFormat:@"%@: %@", content.label, content.value]];
    return [[customContent.get() componentsJoinedByString:@"\n"] createJSStringRef];
#else
    return nullptr;
#endif
}

bool AccessibilityUIElementIOS::boolAttributeValue(JSStringRef attribute)
{
    if (JSStringIsEqualToUTF8CString(attribute, "AXHasTouchEventListener"))
        return [m_element _accessibilityHasTouchEventListener];
    if (JSStringIsEqualToUTF8CString(attribute, "AXIsStrongPasswordField"))
        return [m_element _accessibilityIsStrongPasswordField];
    if (JSStringIsEqualToUTF8CString(attribute, "AXVisited")) {
        UIAccessibilityTraits traits = [m_element accessibilityTraits];
        return (traits & [m_element _axVisitedTrait]) == [m_element _axVisitedTrait];
    }
    return false;
}

bool AccessibilityUIElementIOS::isAttributeSettable(JSStringRef attribute)
{
    return [m_element accessibilityIsAttributeSettable:[NSString stringWithJSStringRef:attribute]];
}

bool AccessibilityUIElementIOS::isAttributeSupported(JSStringRef attribute)
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::parameterizedAttributeNames()
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::role()
{
    return [[m_element _accessibilityWebRoleAsString] createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::subrole()
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::roleDescription()
{
    return concatenateAttributeAndValue(@"AXRoleDescription", [m_element accessibilityRoleDescription]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::computedRoleString()
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::title()
{
    return concatenateAttributeAndValue(@"AXTitle", [m_element accessibilityLabel]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::description()
{
    return concatenateAttributeAndValue(@"AXLabel", [m_element accessibilityLabel]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::brailleLabel() const
{
    return concatenateAttributeAndValue(@"AXBrailleLabel", [m_element accessibilityBrailleLabel]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::brailleRoleDescription() const
{
    return concatenateAttributeAndValue(@"AXBrailleRoleDescription", [m_element accessibilityBrailleRoleDescription]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::liveRegionRelevant() const
{
    return [[m_element accessibilityARIARelevantStatus] createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::liveRegionStatus() const
{
    return [[m_element accessibilityARIALiveRegionStatus] createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::orientation() const
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::stringValue()
{
    return concatenateAttributeAndValue(@"AXValue", [m_element accessibilityValue]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::language()
{
    return concatenateAttributeAndValue(@"AXLanguage", [m_element accessibilityLanguage]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::helpText() const
{
    return concatenateAttributeAndValue(@"AXHint", [m_element accessibilityHint]);
}

double AccessibilityUIElementIOS::pageX()
{
    CGPoint point = [m_element _accessibilityPageRelativeLocation];
    return point.x;
}

double AccessibilityUIElementIOS::pageY()
{
    CGPoint point = [m_element _accessibilityPageRelativeLocation];
    return point.y;
}

double AccessibilityUIElementIOS::x()
{
    CGRect frame = [m_element accessibilityFrame];
    return frame.origin.x;
}

double AccessibilityUIElementIOS::y()
{
    CGRect frame = [m_element accessibilityFrame];
    return frame.origin.y;
}

double AccessibilityUIElementIOS::width()
{
    CGRect frame = [m_element accessibilityFrame];
    return frame.size.width;
}

double AccessibilityUIElementIOS::height()
{
    CGRect frame = [m_element accessibilityFrame];
    return frame.size.height;
}

double AccessibilityUIElementIOS::clickPointX()
{
    return [m_element accessibilityClickPoint].x;
}

double AccessibilityUIElementIOS::clickPointY()
{
    return [m_element accessibilityClickPoint].y;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::lineRectsAndText() const
{
    return [[[m_element lineRectsAndText] componentsJoinedByString:@"|"] createJSStringRef];
}

double AccessibilityUIElementIOS::intValue() const
{
    return [[m_element accessibilityValue] integerValue];
}

double AccessibilityUIElementIOS::minValue()
{
    return [m_element _accessibilityMinValue];
}

double AccessibilityUIElementIOS::maxValue()
{
    return [m_element _accessibilityMaxValue];
}


JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::valueDescription()
{
    return createJSString();
}

int AccessibilityUIElementIOS::insertionPointLineNumber()
{
    return -1;
}

bool AccessibilityUIElementIOS::isPressActionSupported()
{
    return false;
}

bool AccessibilityUIElementIOS::isIncrementActionSupported()
{
    return false;
}

bool AccessibilityUIElementIOS::isDecrementActionSupported()
{
    return false;
}

bool AccessibilityUIElementIOS::isAtomicLiveRegion() const
{
    return [m_element accessibilityARIALiveRegionIsAtomic];
}

bool AccessibilityUIElementIOS::isBusy() const
{
    return [m_element accessibilityARIAIsBusy];
}

bool AccessibilityUIElementIOS::isEnabled()
{
    return false;
}

bool AccessibilityUIElementIOS::isRequired() const
{
    return [m_element accessibilityIsRequired];
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::focusedElement() const
{
    if (id focusedUIElement = [m_element accessibilityFocusedUIElement])
        return AccessibilityUIElement::create(focusedUIElement);
    return nullptr;
}

bool AccessibilityUIElementIOS::isFocused() const
{
    return [m_element _accessibilityIsFocusedForTesting];
}

bool AccessibilityUIElementIOS::isSelected() const
{
    UIAccessibilityTraits traits = [m_element accessibilityTraits];
    return (traits & [m_element _axSelectedTrait]) == [m_element _axSelectedTrait];
}

bool AccessibilityUIElementIOS::isSelectedOptionActive() const
{
    return false;
}

bool AccessibilityUIElementIOS::isIndeterminate() const
{
    return [m_element accessibilityIsIndeterminate];
}

bool AccessibilityUIElementIOS::isExpanded() const
{
    return [m_element accessibilityIsExpanded];
}

bool AccessibilityUIElementIOS::supportsExpanded() const
{
    return [m_element accessibilitySupportsARIAExpanded];
}

bool AccessibilityUIElementIOS::isChecked() const
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::currentStateValue() const
{
    id value = [m_element accessibilityCurrentState];
    if ([value isKindOfClass:[NSString class]])
        return [value createJSStringRef];
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::sortDirection() const
{
    id value = [m_element accessibilitySortDirection];
    if ([value isKindOfClass:[NSString class]])
        return [value createJSStringRef];
    return nullptr;
}

int AccessibilityUIElementIOS::hierarchicalLevel() const
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::classList() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::speakAs()
{
    return [[[m_element accessibilitySpeechHint] componentsJoinedByString:@", "] createJSStringRef];
}

bool AccessibilityUIElementIOS::isGrabbed() const
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::ariaDropEffects() const
{
    return createJSString();
}

// parameterized attributes
int AccessibilityUIElementIOS::lineForIndex(int index)
{
    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::rangeForLine(int line)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::rangeForPosition(int x, int y)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::boundsForRange(unsigned location, unsigned length)
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::stringForRange(unsigned location, unsigned length)
{
    NSString *stringForRange = [m_element stringForRange:NSMakeRange(location, length)];
    return [stringForRange createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributedStringForRange(unsigned location, unsigned length)
{
    NSAttributedString *stringForRange = [m_element attributedStringForRange:NSMakeRange(location, length)];
    return [[stringForRange description] createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributedStringForElement()
{
    NSAttributedString *string = [m_element attributedStringForElement];
    if (![string isKindOfClass:[NSAttributedString class]])
        return nullptr;

    return [[string description] createJSStringRef];
}

bool AccessibilityUIElementIOS::attributedStringRangeIsMisspelled(unsigned location, unsigned length)
{
    return false;
}

unsigned AccessibilityUIElementIOS::uiElementCountForSearchPredicate(JSContextRef context, AccessibilityUIElement *startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    return 0;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement *startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    NSDictionary *parameter = searchPredicateForSearchCriteria(context, startElement, nullptr, isDirectionNext, 5, searchKey, searchText, visibleOnly, immediateDescendantsOnly);
    id results = [m_element accessibilityFindMatchingObjects:parameter];
    if (![results isKindOfClass:[NSArray class]])
        return nullptr;

    for (id element in results) {
        if ([element isAccessibilityElement])
            return AccessibilityUIElement::create(element);
    }

    if (id firstResult = [results firstObject])
        return AccessibilityUIElement::create(firstResult);
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::selectTextWithCriteria(JSContextRef, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributesOfColumnHeaders()
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributesOfRowHeaders()
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributesOfColumns()
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributesOfRows()
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributesOfVisibleCells()
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributesOfHeader()
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::traits()
{
    return concatenateAttributeAndValue(@"AXTraits", [NSString stringWithFormat:@"%qu", [m_element accessibilityTraits]]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::identifier()
{
    return concatenateAttributeAndValue(@"AXIdentifier", [m_element accessibilityIdentifier]);
}

bool AccessibilityUIElementIOS::hasTextEntryTrait()
{
    UIAccessibilityTraits traits = [m_element accessibilityTraits];
    return (traits & [m_element _axTextEntryTrait]) == [m_element _axTextEntryTrait];
}

bool AccessibilityUIElementIOS::hasTabBarTrait()
{
    UIAccessibilityTraits traits = [m_element accessibilityTraits];
    return (traits & [m_element _axTabBarTrait]) == [m_element _axTabBarTrait];
}

bool AccessibilityUIElementIOS::hasMenuItemTrait()
{
    UIAccessibilityTraits traits = [m_element accessibilityTraits];
    return (traits & [m_element _axMenuItemTrait]) == [m_element _axMenuItemTrait];
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::fieldsetAncestorElement()
{
    id ancestorElement = [m_element _accessibilityFieldsetAncestor];
    if (ancestorElement)
        return AccessibilityUIElement::create(ancestorElement);

    return nullptr;
}

bool AccessibilityUIElementIOS::isTextArea() const
{
    return ([m_element accessibilityTraits] & [m_element _axTextAreaTrait]) == [m_element _axTextAreaTrait];
}

bool AccessibilityUIElementIOS::isSearchField() const
{
    return ([m_element accessibilityTraits] & [m_element _axSearchFieldTrait]) == [m_element _axSearchFieldTrait];
}

bool AccessibilityUIElementIOS::isSwitch() const
{
    return [m_element _accessibilityIsSwitch];
}

int AccessibilityUIElementIOS::rowCount()
{
    return [m_element accessibilityRowCount];
}

int AccessibilityUIElementIOS::columnCount()
{
    return [m_element accessibilityColumnCount];
}

bool AccessibilityUIElementIOS::isInCell() const
{
    return [m_element _accessibilityIsInTableCell];
}

bool AccessibilityUIElementIOS::isInTable() const
{
    return [m_element _accessibilityTableAncestor] != nullptr;
}

bool AccessibilityUIElementIOS::isInLandmark() const
{
    return [m_element _accessibilityLandmarkAncestor] != nullptr;
}

bool AccessibilityUIElementIOS::isInList() const
{
    return [m_element _accessibilityListAncestor] != nullptr;
}

int AccessibilityUIElementIOS::indexInTable()
{
    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::rowIndexRange()
{
    NSRange range = [m_element accessibilityRowRange];
    NSMutableString* rangeDescription = [NSMutableString stringWithFormat:@"{%lu, %lu}", (unsigned long)range.location, (unsigned long)range.length];
    return [rangeDescription createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::columnIndexRange()
{
    NSRange range = [m_element accessibilityColumnRange];
    NSMutableString* rangeDescription = [NSMutableString stringWithFormat:@"{%lu, %lu}", (unsigned long)range.location, (unsigned long)range.length];
    return [rangeDescription createJSStringRef];
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::cellForColumnAndRow(unsigned col, unsigned row)
{
    return AccessibilityUIElement::create([m_element accessibilityElementForRow:row andColumn:col]);
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::horizontalScrollbar() const
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::verticalScrollbar() const
{
    return nullptr;
}

void AccessibilityUIElementIOS::scrollToMakeVisible()
{
}

void AccessibilityUIElementIOS::scrollToGlobalPoint(int x, int y)
{
}

void AccessibilityUIElementIOS::scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height)
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::selectedTextRange()
{
    NSRange range = [m_element _accessibilitySelectedTextRange];
    NSMutableString *rangeDescription = [NSMutableString stringWithFormat:@"{%lu, %lu}", static_cast<unsigned long>(range.location), static_cast<unsigned long>(range.length)];
    return [rangeDescription createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::intersectionWithSelectionRange()
{
    return nullptr;
}

bool AccessibilityUIElementIOS::setSelectedTextMarkerRange(AccessibilityTextMarkerRange*)
{
    return false;
}

bool AccessibilityUIElementIOS::setSelectedTextRange(unsigned location, unsigned length)
{
    [m_element _accessibilitySetSelectedTextRange:NSMakeRange(location, length)];
    return true;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::textInputMarkedRange() const
{
    return WTR::createJSString();
}

void AccessibilityUIElementIOS::increment()
{
    [m_element accessibilityIncrement];
}

void AccessibilityUIElementIOS::decrement()
{
    [m_element accessibilityDecrement];
}

void AccessibilityUIElementIOS::showMenu()
{
}

void AccessibilityUIElementIOS::press()
{
    [m_element _accessibilityActivate];
}

bool AccessibilityUIElementIOS::dismiss()
{
    return [m_element accessibilityPerformEscape];
}

void AccessibilityUIElementIOS::setSelectedChild(AccessibilityUIElement* element) const
{
}

void AccessibilityUIElementIOS::setSelectedChildAtIndex(unsigned index) const
{
}

void AccessibilityUIElementIOS::removeSelectionAtIndex(unsigned index) const
{
}

void AccessibilityUIElementIOS::clearSelectedChildren() const
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::accessibilityValue() const
{
    return createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::dateTimeValue() const
{
    return [[m_element accessibilityDatetimeValue] createJSStringRef];
}

void AccessibilityUIElementIOS::assistiveTechnologySimulatedFocus()
{
    [m_element accessibilityElementDidBecomeFocused];
}

bool AccessibilityUIElementIOS::scrollPageUp()
{
    return [m_element accessibilityScroll:UIAccessibilityScrollDirectionUp];
}

bool AccessibilityUIElementIOS::scrollPageDown()
{
    return [m_element accessibilityScroll:UIAccessibilityScrollDirectionDown];
}
bool AccessibilityUIElementIOS::scrollPageLeft()
{
    return [m_element accessibilityScroll:UIAccessibilityScrollDirectionLeft];
}

bool AccessibilityUIElementIOS::scrollPageRight()
{
    return [m_element accessibilityScroll:UIAccessibilityScrollDirectionRight];
}

void AccessibilityUIElementIOS::increaseTextSelection()
{
    [m_element accessibilityModifySelection:WebCore::TextGranularity::CharacterGranularity increase:YES];
}

void AccessibilityUIElementIOS::decreaseTextSelection()
{
    [m_element accessibilityModifySelection:WebCore::TextGranularity::CharacterGranularity increase:NO];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::stringForSelection()
{
    NSString *stringForRange = [m_element selectionRangeString];
    return [stringForRange createJSStringRef];
}

int AccessibilityUIElementIOS::elementTextPosition()
{
    NSRange range = [[m_element valueForKey:@"elementTextRange"] rangeValue];
    return range.location;
}

int AccessibilityUIElementIOS::elementTextLength()
{
    NSRange range = [[m_element valueForKey:@"elementTextRange"] rangeValue];
    return range.length;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::url()
{
    NSURL *url = [m_element accessibilityURL];
    return [[url absoluteString] createJSStringRef];
}

bool AccessibilityUIElementIOS::addNotificationListener(JSContextRef context, JSValueRef functionCallback)
{
    if (!functionCallback)
        return false;

    // iOS programmers should not be adding more than one notification listener per element.
    // Other platforms may be different.
    if (m_notificationHandler)
        return false;

    m_notificationHandler = adoptNS([[AccessibilityNotificationHandler alloc] initWithContext:context]);
    [m_notificationHandler setPlatformElement:platformUIElement()];
    [m_notificationHandler setCallback:functionCallback];
    [m_notificationHandler startObserving];

    return true;
}

bool AccessibilityUIElementIOS::removeNotificationListener()
{
    // iOS programmers should not be trying to remove a listener that's already removed.
    ASSERT(m_notificationHandler);

    [m_notificationHandler stopObserving];
    m_notificationHandler = nil;

    return true;
}

bool AccessibilityUIElementIOS::isFocusable() const
{
    return false;
}

bool AccessibilityUIElementIOS::isSelectable() const
{
    return false;
}

bool AccessibilityUIElementIOS::isMultiSelectable() const
{
    return false;
}

bool AccessibilityUIElementIOS::isVisible() const
{
    return false;
}

bool AccessibilityUIElementIOS::isOffScreen() const
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::embeddedImageDescription() const
{
    return concatenateAttributeAndValue(@"AXEmbeddedImageDescription", [m_element _accessibilityPhotoDescription]);
}

JSValueRef AccessibilityUIElementIOS::imageOverlayElements(JSContextRef context)
{
    return makeJSArray(context, makeVector<RefPtr<AccessibilityUIElement>>([m_element accessibilityImageOverlayElements]));
}

bool AccessibilityUIElementIOS::isCollapsed() const
{
    return false;
}

bool AccessibilityUIElementIOS::isIgnored() const
{
    bool isAccessibilityElement = [m_element isAccessibilityElement];
    return !isAccessibilityElement;
}

bool AccessibilityUIElementIOS::isSingleLine() const
{
    return false;
}

bool AccessibilityUIElementIOS::isMultiLine() const
{
    return false;
}

bool AccessibilityUIElementIOS::hasPopup() const
{
    return [m_element accessibilityHasPopup];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::popupValue() const
{
    return [[m_element accessibilityPopupValue] createJSStringRef];
}

bool AccessibilityUIElementIOS::isInDescriptionListDetail() const
{
    // The names are inconsistent here (isInDescriptionListDetail vs. isInDescriptionListDefinition)
    // because the iOS interface requires the latter form.
    return [m_element accessibilityIsInDescriptionListDefinition];
}

bool AccessibilityUIElementIOS::isInDescriptionListTerm() const
{
    return [m_element accessibilityIsInDescriptionListTerm];
}

void AccessibilityUIElementIOS::takeFocus()
{
    [m_element _accessibilitySetFocus:YES];
}

void AccessibilityUIElementIOS::takeSelection()
{
}

void AccessibilityUIElementIOS::addSelection()
{
}

void AccessibilityUIElementIOS::removeSelection()
{
}

// Text markers
RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementIOS::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    id startTextMarker = [m_element lineStartMarkerForMarker:textMarker->platformTextMarker()];
    id endTextMarker = [m_element lineEndMarkerForMarker:textMarker->platformTextMarker()];
    NSArray *textMarkers = @[startTextMarker, endTextMarker];

    id textMarkerRange = [m_element textMarkerRangeForMarkers:textMarkers];
    return AccessibilityTextMarkerRange::create(textMarkerRange);
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementIOS::textMarkerRangeForSearchPredicate(JSContextRef context, AccessibilityTextMarkerRange *startRange, bool forward, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    // FIXME: add implementation for iOS.
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementIOS::misspellingTextMarkerRange(AccessibilityTextMarkerRange* start, bool forward)
{
    id misspellingRange = [m_element misspellingTextMarkerRange:start->platformTextMarkerRange() forward:forward];
    return AccessibilityTextMarkerRange::create(misspellingRange);
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementIOS::textMarkerRangeForElement(AccessibilityUIElement* element)
{
    if (!element)
        return nullptr;

    id textMarkerRange = [element->platformUIElement() textMarkerRange];
    return AccessibilityTextMarkerRange::create(textMarkerRange);
}

int AccessibilityUIElementIOS::textMarkerRangeLength(AccessibilityTextMarkerRange* range)
{
    return [m_element lengthForTextMarkers:range->platformTextMarkerRange()];
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::previousTextMarker(AccessibilityTextMarker* textMarker)
{
    id previousMarker = [m_element previousMarkerForMarker:textMarker->platformTextMarker()];
    return AccessibilityTextMarker::create(previousMarker);
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::nextTextMarker(AccessibilityTextMarker* textMarker)
{
    id nextMarker = [m_element nextMarkerForMarker:textMarker->platformTextMarker()];
    return AccessibilityTextMarker::create(nextMarker);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::stringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    id textMarkers = markerRange->platformTextMarkerRange();
    if (![textMarkers isKindOfClass:[NSArray class]])
        return createJSString();
    return [[m_element stringForTextMarkers:textMarkers] createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::rectsForTextMarkerRange(AccessibilityTextMarkerRange* markerRange, JSStringRef text)
{
    id textMarkers = markerRange->platformTextMarkerRange();
    if (![textMarkers isKindOfClass:[NSArray class]])
        return createJSString();
    return [[[m_element textRectsFromMarkers:textMarkers withText:[NSString stringWithJSStringRef:text]] componentsJoinedByString:@","] createJSStringRef];
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementIOS::textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    NSArray *textMarkers = @[startMarker->platformTextMarker(), endMarker->platformTextMarker()];
    id textMarkerRange = [m_element textMarkerRangeForMarkers:textMarkers];
    return AccessibilityTextMarkerRange::create(textMarkerRange);
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    id textMarkers = range->platformTextMarkerRange();
    id textMarker = [m_element startOrEndTextMarkerForTextMarkers:textMarkers isStart:YES];
    return AccessibilityTextMarker::create(textMarker);
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    id textMarkers = range->platformTextMarkerRange();
    id textMarker = [m_element startOrEndTextMarkerForTextMarkers:textMarkers isStart:NO];
    return AccessibilityTextMarker::create(textMarker);
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::endTextMarkerForBounds(int x, int y, int width, int height)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::startTextMarkerForBounds(int x, int y, int width, int height)
{
    return nullptr;
}

bool AccessibilityUIElementIOS::replaceTextInRange(JSStringRef string, int location, int length)
{
    return [m_element accessibilityReplaceRange:NSMakeRange(location, length) withText:[NSString stringWithJSStringRef:string]];
}

bool AccessibilityUIElementIOS::insertText(JSStringRef text)
{
    return [m_element accessibilityInsertText:[NSString stringWithJSStringRef:text]];
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::textMarkerForPoint(int x, int y)
{
    return nullptr;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementIOS::accessibilityElementForTextMarker(AccessibilityTextMarker* marker)
{
    id obj = [m_element accessibilityObjectForTextMarker:marker->platformTextMarker()];
    if (obj)
        return AccessibilityUIElement::create(obj);
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    if (!markerRange)
        return nullptr;

    NSAttributedString *stringForRange = [m_element _attributedStringForTextMarkerRangeForTesting:markerRange->platformTextMarkerRange()];
    return [[stringForRange description] createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributedStringForTextMarkerRangeWithDidSpellCheck(AccessibilityTextMarkerRange* markerRange)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange* markerRange, bool)
{
    return nullptr;
}

bool AccessibilityUIElementIOS::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef attribute, AccessibilityTextMarkerRange* range)
{
    return false;
}

int AccessibilityUIElementIOS::indexForTextMarker(AccessibilityTextMarker* marker)
{
    return [m_element positionForTextMarker:(__bridge id)marker->platformTextMarker()];
}

bool AccessibilityUIElementIOS::isTextMarkerValid(AccessibilityTextMarker* textMarker)
{
    return false;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::textMarkerForIndex(int textIndex)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::startTextMarker()
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::endTextMarker()
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementIOS::leftWordTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementIOS::rightWordTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::previousWordStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::nextWordEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementIOS::paragraphTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::nextParagraphEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::previousParagraphStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementIOS::sentenceTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::nextSentenceEndTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarker> AccessibilityUIElementIOS::previousSentenceStartTextMarkerForTextMarker(AccessibilityTextMarker* textMarker)
{
    return nullptr;
}

RefPtr<AccessibilityTextMarkerRange> AccessibilityUIElementIOS::textMarkerRangeMatchesTextNearMarkers(JSStringRef text, AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    NSArray *textMarkers = nil;
    if (startMarker->platformTextMarker() && endMarker->platformTextMarker())
        textMarkers = @[startMarker->platformTextMarker(), endMarker->platformTextMarker()];
    id textMarkerRange = [m_element textMarkerRangeFromMarkers:textMarkers withText:[NSString stringWithJSStringRef:text]];
    return AccessibilityTextMarkerRange::create(textMarkerRange);
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::mathPostscriptsDescription() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::mathPrescriptsDescription() const
{
    return nullptr;
}

static void _CGPathEnumerationIteration(void *info, const CGPathElement *element)
{
    NSMutableString *result = (NSMutableString *)info;
    switch (element->type) {
    case kCGPathElementMoveToPoint:
        [result appendString:@"\tMove to point\n"];
        break;
    case kCGPathElementAddLineToPoint:
        [result appendString:@"\tLine to\n"];
        break;
    case kCGPathElementAddQuadCurveToPoint:
        [result appendString:@"\tQuad curve to\n"];
        break;
    case kCGPathElementAddCurveToPoint:
        [result appendString:@"\tCurve to\n"];
        break;
    case kCGPathElementCloseSubpath:
        [result appendString:@"\tClose\n"];
        break;
    }
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::pathDescription() const
{
    NSMutableString *result = [NSMutableString stringWithString:@"\nStart Path\n"];
    CGPathRef pathRef = [m_element _accessibilityPath];

    CGPathApply(pathRef, result, _CGPathEnumerationIteration);

    return [result createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElementIOS::supportedActions() const
{
    return nullptr;
}

bool AccessibilityUIElementIOS::isInsertion() const
{
    return [m_element accessibilityIsInsertion];
}

bool AccessibilityUIElementIOS::isDeletion() const
{
    return [m_element accessibilityIsDeletion];
}

bool AccessibilityUIElementIOS::isFirstItemInSuggestion() const
{
    return [m_element accessibilityIsFirstItemInSuggestion];
}

bool AccessibilityUIElementIOS::isLastItemInSuggestion() const
{
    return [m_element accessibilityIsLastItemInSuggestion];
}

bool AccessibilityUIElementIOS::isMarkAnnotation() const
{
    return [m_element accessibilityIsMarkAnnotation];
}

} // namespace WTR
