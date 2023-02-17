/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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
#import <WebCore/TextGranularity.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebHTMLView.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#import <UIKit/UIKit.h>

typedef void (*AXPostedNotificationCallback)(id element, NSString* notification, void* context);

@interface NSObject (UIAccessibilityHidden)
- (id)accessibilityHitTest:(CGPoint)point;
- (id)accessibilityLinkedElement;
- (NSRange)accessibilityColumnRange;
- (NSRange)accessibilityRowRange;
- (id)accessibilityElementForRow:(NSInteger)row andColumn:(NSInteger)column;
- (NSURL *)accessibilityURL;
- (NSArray *)accessibilityHeaderElements;
- (NSString *)accessibilityPlaceholderValue;
- (NSString *)stringForRange:(NSRange)range;
- (NSAttributedString *)attributedStringForRange:(NSRange)range;
- (NSAttributedString *)attributedStringForElement;
- (NSString *)selectionRangeString;
- (BOOL)accessibilityInsertText:(NSString *)text;
- (CGPoint)accessibilityClickPoint;
- (void)accessibilityModifySelection:(WebCore::TextGranularity)granularity increase:(BOOL)increase;
- (void)accessibilitySetPostedNotificationCallback:(AXPostedNotificationCallback)function withContext:(void*)context;
- (CGFloat)_accessibilityMinValue;
- (CGFloat)_accessibilityMaxValue;
- (void)_accessibilitySetValue:(NSString *)value;
- (void)_accessibilityActivate;
- (UIAccessibilityTraits)_axSelectedTrait;
- (UIAccessibilityTraits)_axTextAreaTrait;
- (UIAccessibilityTraits)_axSearchFieldTrait;
- (NSString *)accessibilityARIACurrentStatus;
- (NSUInteger)accessibilityRowCount;
- (NSUInteger)accessibilityColumnCount;
- (NSUInteger)accessibilityARIARowCount;
- (NSUInteger)accessibilityARIAColumnCount;
- (NSUInteger)accessibilityARIARowIndex;
- (NSUInteger)accessibilityARIAColumnIndex;
- (BOOL)accessibilityARIAIsBusy;
- (BOOL)accessibilityARIALiveRegionIsAtomic;
- (NSString *)accessibilityARIALiveRegionStatus;
- (NSString *)accessibilityARIARelevantStatus;
- (UIAccessibilityTraits)_axContainedByFieldsetTrait;
- (id)_accessibilityFieldsetAncestor;
- (BOOL)_accessibilityHasTouchEventListener;
- (NSString *)accessibilityExpandedTextValue;
- (NSString *)accessibilitySortDirection;
- (BOOL)accessibilityIsExpanded;
- (NSUInteger)accessibilityBlockquoteLevel;
- (NSArray *)accessibilityFindMatchingObjects:(NSDictionary *)parameters;
- (NSArray *)accessibilitySpeechHint;
- (BOOL)_accessibilityIsStrongPasswordField;
- (NSString *)accessibilityTextualContext;
- (BOOL)accessibilityHasPopup;
- (NSString *)accessibilityPopupValue;
- (BOOL)accessibilityHasDocumentRoleAncestor;
- (BOOL)accessibilityHasWebApplicationAncestor;
- (BOOL)accessibilityIsInDescriptionListDefinition;
- (BOOL)accessibilityIsInDescriptionListTerm;
- (BOOL)_accessibilityIsInTableCell;
- (BOOL)accessibilityIsAttributeSettable:(NSString *)attributeName;
- (BOOL)accessibilityIsRequired;
- (NSString *)_accessibilityPhotoDescription;
- (BOOL)accessibilityPerformEscape;
- (NSString *)accessibilityDOMIdentifier;
- (NSString *)_accessibilityWebRoleAsString;
- (BOOL)accessibilityIsInsertion;
- (BOOL)accessibilityIsDeletion;
- (BOOL)accessibilityIsFirstItemInSuggestion;
- (BOOL)accessibilityIsLastItemInSuggestion;
- (BOOL)accessibilityIsMarkAnnotation;

// TextMarker related
- (NSArray *)textMarkerRange;
- (NSInteger)lengthForTextMarkers:(NSArray *)textMarkers;
- (NSString *)stringForTextMarkers:(NSArray *)markers;
- (id)startOrEndTextMarkerForTextMarkers:(NSArray*)textMarkers isStart:(BOOL)isStart;
- (NSArray *)textMarkerRangeForMarkers:(NSArray *)textMarkers;
- (id)nextMarkerForMarker:(id)marker;
- (id)previousMarkerForMarker:(id)marker;
- (id)accessibilityObjectForTextMarker:(id)marker;
- (id)lineStartMarkerForMarker:(id)marker;
- (id)lineEndMarkerForMarker:(id)marker;
- (NSArray *)misspellingTextMarkerRange:(NSArray *)startTextMarkerRange forward:(BOOL)forward;
- (NSArray *)textMarkerRangeFromMarkers:(NSArray *)markers withText:(NSString *)text;
@end

@interface NSObject (WebAccessibilityObjectWrapperPrivate)
- (CGPathRef)_accessibilityPath;
@end

AccessibilityUIElement::AccessibilityUIElement(id element)
    : m_element(element)
{
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

#pragma mark iPhone Attributes

JSRetainPtr<JSStringRef> AccessibilityUIElement::identifier()
{
    return concatenateAttributeAndValue(@"AXIdentifier", [m_element accessibilityIdentifier]);
}

bool AccessibilityUIElement::isTextArea() const
{
    return ([m_element accessibilityTraits] & [m_element _axTextAreaTrait]) == [m_element _axTextAreaTrait];
}

bool AccessibilityUIElement::isSearchField() const
{
    return ([m_element accessibilityTraits] & [m_element _axSearchFieldTrait]) == [m_element _axSearchFieldTrait];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::traits()
{
    return concatenateAttributeAndValue(@"AXTraits", [NSString stringWithFormat:@"%qu", [m_element accessibilityTraits]]);
}

int AccessibilityUIElement::elementTextPosition()
{
    NSRange range = [[m_element valueForKey:@"elementTextRange"] rangeValue];
    return range.location;
}

int AccessibilityUIElement::elementTextLength()
{
    NSRange range = [[m_element valueForKey:@"elementTextRange"] rangeValue];
    return range.length;    
}

bool AccessibilityUIElement::hasContainedByFieldsetTrait()
{
    UIAccessibilityTraits traits = [m_element accessibilityTraits];
    return (traits & [m_element _axContainedByFieldsetTrait]) == [m_element _axContainedByFieldsetTrait];
}

AccessibilityUIElement AccessibilityUIElement::fieldsetAncestorElement()
{
    id ancestorElement = [m_element _accessibilityFieldsetAncestor];
    if (ancestorElement)
        return AccessibilityUIElement(ancestorElement);
    
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::url()
{
    NSURL *url = [m_element accessibilityURL];
    return [[url absoluteString] createJSStringRef];
}

double AccessibilityUIElement::x()
{
    CGRect frame = [m_element accessibilityFrame];
    return frame.origin.x;
}

double AccessibilityUIElement::y()
{
    CGRect frame = [m_element accessibilityFrame];
    return frame.origin.y;
}

double AccessibilityUIElement::width()
{
    CGRect frame = [m_element accessibilityFrame];
    return frame.size.width;
}

double AccessibilityUIElement::height()
{
    CGRect frame = [m_element accessibilityFrame];
    return frame.size.height;
}

double AccessibilityUIElement::clickPointX()
{
    CGPoint centerPoint = [m_element accessibilityClickPoint];
    return centerPoint.x;
}

double AccessibilityUIElement::clickPointY()
{
    CGPoint centerPoint = [m_element accessibilityClickPoint];
    return centerPoint.y;
}

void AccessibilityUIElement::getChildren(Vector<AccessibilityUIElement>& elementVector)
{
    NSInteger childCount = [m_element accessibilityElementCount];
    for (NSInteger k = 0; k < childCount; ++k)
        elementVector.append(AccessibilityUIElement([m_element accessibilityElementAtIndex:k]));
}

void AccessibilityUIElement::getChildrenWithRange(Vector<AccessibilityUIElement>& elementVector, unsigned location, unsigned length)
{
    // accessibilityElementAtIndex: takes an NSInteger.
    // We want to preserve that in order to test against invalid indexes being input.
    NSInteger maxValue = static_cast<NSInteger>(location + length);
    for (NSInteger k = location; k < maxValue; ++k)
        elementVector.append(AccessibilityUIElement([m_element accessibilityElementAtIndex:k]));    
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
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::getChildAtIndex(unsigned index)
{
    Vector<AccessibilityUIElement> children;
    getChildrenWithRange(children, index, 1);
    
    if (children.size() == 1)
        return children[0];
    return nil;
}

AccessibilityUIElement AccessibilityUIElement::headerElementAtIndex(unsigned index)
{
    NSArray *headers = [m_element accessibilityHeaderElements];
    if (index < [headers count])
        return [headers objectAtIndex:index];
    
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::linkedElement()
{
    id linkedElement = [m_element accessibilityLinkedElement];
    if (linkedElement)
        return AccessibilityUIElement(linkedElement);
    
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::linkedUIElementAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::ariaOwnsElementAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::ariaFlowToElementAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::ariaControlsElementAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::disclosedRowAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::selectedRowAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::rowAtIndex(unsigned index)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::titleUIElement()
{
    return 0;
}

void AccessibilityUIElement::dismiss()
{
    [m_element accessibilityPerformEscape];
}

AccessibilityUIElement AccessibilityUIElement::parentElement()
{
    id accessibilityObject = [m_element accessibilityContainer];
    if (accessibilityObject)
        return AccessibilityUIElement(accessibilityObject);
    
    return nil;
}

AccessibilityUIElement AccessibilityUIElement::disclosedByRow()
{
    return 0;
}

void AccessibilityUIElement::increaseTextSelection()
{
    [m_element accessibilityModifySelection:WebCore::TextGranularity::CharacterGranularity increase:YES];
}

void AccessibilityUIElement::decreaseTextSelection()
{
    [m_element accessibilityModifySelection:WebCore::TextGranularity::CharacterGranularity increase:NO];    
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::speakAs()
{
    return [[[m_element accessibilitySpeechHint] componentsJoinedByString:@", "] createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForSelection()
{ 
    NSString *stringForRange = [m_element selectionRangeString];
    return [stringForRange createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForRange(unsigned location, unsigned length)
{ 
    NSString *stringForRange = [m_element stringForRange:NSMakeRange(location, length)];
    return [stringForRange createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForRange(unsigned location, unsigned length)
{
    NSRange range = NSMakeRange(location, length);
    NSAttributedString* string = [m_element attributedStringForRange:range];
    if (![string isKindOfClass:[NSAttributedString class]])
        return 0;
    
    NSString* stringWithAttrs = [string description];
    return [stringWithAttrs createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForElement()
{
    NSAttributedString *string = [m_element attributedStringForElement];
    if (![string isKindOfClass:[NSAttributedString class]])
        return nullptr;
    
    return [[string description] createJSStringRef];
}

bool AccessibilityUIElement::attributedStringRangeIsMisspelled(unsigned, unsigned)
{
    return false;
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

JSRetainPtr<JSStringRef> AccessibilityUIElement::pathDescription() const
{
    NSMutableString *result = [NSMutableString stringWithString:@"\nStart Path\n"];
    CGPathRef pathRef = [m_element _accessibilityPath];
    
    CGPathApply(pathRef, result, _CGPathEnumerationIteration);
    
    return [result createJSStringRef];
}

#if SUPPORTS_AX_TEXTMARKERS && PLATFORM(IOS_FAMILY)

// Text markers

AccessibilityTextMarkerRange AccessibilityUIElement::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker* textMarker)
{
    id startTextMarker = [m_element lineStartMarkerForMarker:textMarker->platformTextMarker()];
    id endTextMarker = [m_element lineEndMarkerForMarker:textMarker->platformTextMarker()];
    NSArray *textMarkers = @[startTextMarker, endTextMarker];
    
    id textMarkerRange = [m_element textMarkerRangeForMarkers:textMarkers];
    return AccessibilityTextMarkerRange(textMarkerRange);
}

AccessibilityTextMarkerRange AccessibilityUIElement::misspellingTextMarkerRange(AccessibilityTextMarkerRange* startRange, bool forward)
{
    id misspellingRange = [m_element misspellingTextMarkerRange:startRange->platformTextMarkerRange() forward:forward];
    return AccessibilityTextMarkerRange(misspellingRange);
}

AccessibilityTextMarkerRange AccessibilityUIElement::textMarkerRangeForElement(AccessibilityUIElement* element)
{
    id textMarkerRange = [element->platformUIElement() textMarkerRange];
    return AccessibilityTextMarkerRange(textMarkerRange);
}

AccessibilityTextMarkerRange AccessibilityUIElement::selectedTextMarkerRange()
{
    return nullptr;
}

bool AccessibilityUIElement::replaceTextInRange(JSStringRef, int, int)
{
    return false;
}

bool AccessibilityUIElement::insertText(JSStringRef text)
{
    return [m_element accessibilityInsertText:[NSString stringWithJSStringRef:text]];
}

void AccessibilityUIElement::resetSelectedTextMarkerRange()
{
}

int AccessibilityUIElement::textMarkerRangeLength(AccessibilityTextMarkerRange* range)
{
    id textMarkers = range->platformTextMarkerRange();
    return [m_element lengthForTextMarkers:textMarkers];
}

AccessibilityTextMarkerRange AccessibilityUIElement::textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    NSArray *textMarkers = @[startMarker->platformTextMarker(), endMarker->platformTextMarker()];
    id textMarkerRange = [m_element textMarkerRangeForMarkers:textMarkers];
    return AccessibilityTextMarkerRange(textMarkerRange);
}

AccessibilityTextMarker AccessibilityUIElement::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    id textMarkers = range->platformTextMarkerRange();
    id textMarker = [m_element startOrEndTextMarkerForTextMarkers:textMarkers isStart:YES];
    return AccessibilityTextMarker(textMarker);
}

AccessibilityTextMarker AccessibilityUIElement::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    id textMarkers = range->platformTextMarkerRange();
    id textMarker = [m_element startOrEndTextMarkerForTextMarkers:textMarkers isStart:NO];
    return AccessibilityTextMarker(textMarker);
}

AccessibilityUIElement AccessibilityUIElement::accessibilityElementForTextMarker(AccessibilityTextMarker* marker)
{
    id obj = [m_element accessibilityObjectForTextMarker:marker->platformTextMarker()];
    if (obj)
        return AccessibilityUIElement(obj);
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::endTextMarkerForBounds(int x, int y, int width, int height)
{
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::startTextMarkerForBounds(int x, int y, int width, int height)
{
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::textMarkerForPoint(int x, int y)
{
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::previousTextMarker(AccessibilityTextMarker* textMarker)
{
    id previousMarker = [m_element previousMarkerForMarker:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(previousMarker);
}

AccessibilityTextMarker AccessibilityUIElement::nextTextMarker(AccessibilityTextMarker* textMarker)
{
    id nextMarker = [m_element nextMarkerForMarker:textMarker->platformTextMarker()];
    return AccessibilityTextMarker(nextMarker);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    id textMarkers = markerRange->platformTextMarkerRange();
    if (!textMarkers || ![textMarkers isKindOfClass:[NSArray class]])
        return WTR::createJSString();
    return [[m_element stringForTextMarkers:textMarkers] createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool)
{
    return nullptr;
}

bool AccessibilityUIElement::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef, AccessibilityTextMarkerRange*)
{
    return false;
}

int AccessibilityUIElement::indexForTextMarker(AccessibilityTextMarker*)
{
    return -1;
}

bool AccessibilityUIElement::isTextMarkerNull(AccessibilityTextMarker*)
{
    return true;
}

bool AccessibilityUIElement::isTextMarkerValid(AccessibilityTextMarker*)
{
    return false;
}

AccessibilityTextMarker AccessibilityUIElement::textMarkerForIndex(int)
{
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::startTextMarker()
{
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::endTextMarker()
{
    return nullptr;
}

bool AccessibilityUIElement::setSelectedTextMarkerRange(AccessibilityTextMarkerRange*)
{
    return false;
}

AccessibilityTextMarkerRange AccessibilityUIElement::leftWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*)
{
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::rightWordTextMarkerRangeForTextMarker(AccessibilityTextMarker*)
{
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::previousWordStartTextMarkerForTextMarker(AccessibilityTextMarker*)
{
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::nextWordEndTextMarkerForTextMarker(AccessibilityTextMarker*)
{
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::paragraphTextMarkerRangeForTextMarker(AccessibilityTextMarker*)
{
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::previousParagraphStartTextMarkerForTextMarker(AccessibilityTextMarker*)
{
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::nextParagraphEndTextMarkerForTextMarker(AccessibilityTextMarker*)
{
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::sentenceTextMarkerRangeForTextMarker(AccessibilityTextMarker*)
{
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::previousSentenceStartTextMarkerForTextMarker(AccessibilityTextMarker*)
{
    return nullptr;
}

AccessibilityTextMarker AccessibilityUIElement::nextSentenceEndTextMarkerForTextMarker(AccessibilityTextMarker*)
{
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::textMarkerRangeMatchesTextNearMarkers(JSStringRef text, AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    NSArray *textMarkers = nil;
    if (startMarker->platformTextMarker() && endMarker->platformTextMarker())
        textMarkers = @[startMarker->platformTextMarker(), endMarker->platformTextMarker()];
    id textMarkerRange = [m_element textMarkerRangeFromMarkers:textMarkers withText:[NSString stringWithJSStringRef:text]];
    return AccessibilityTextMarkerRange(textMarkerRange);
}


#endif // SUPPORTS_AX_TEXTMARKERS && PLATFORM(IOS_FAMILY)

#pragma mark Unused

void AccessibilityUIElement::getLinkedUIElements(Vector<AccessibilityUIElement>& elementVector)
{
}

void AccessibilityUIElement::getDocumentLinks(Vector<AccessibilityUIElement>& elementVector)
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfLinkedUIElements()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfDocumentLinks()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfChildren()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::allAttributes()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(JSStringRef attribute)
{
    if (JSStringIsEqualToUTF8CString(attribute, "AXPlaceholderValue"))
        return [[m_element accessibilityPlaceholderValue] createJSStringRef];
    
    if (JSStringIsEqualToUTF8CString(attribute, "AXARIACurrent"))
        return [[m_element accessibilityARIACurrentStatus] createJSStringRef];

    if (JSStringIsEqualToUTF8CString(attribute, "AXExpandedTextValue"))
        return [[m_element accessibilityExpandedTextValue] createJSStringRef];
    
    if (JSStringIsEqualToUTF8CString(attribute, "AXSortDirection"))
        return [[m_element accessibilitySortDirection] createJSStringRef];
    
    if (JSStringIsEqualToUTF8CString(attribute, "AXTextualContext"))
        return [[m_element accessibilityTextualContext] createJSStringRef];
    
    return WTR::createJSString();
}

bool AccessibilityUIElement::isPressActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isIncrementActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isDecrementActionSupported()
{
    return false;
}

bool AccessibilityUIElement::boolAttributeValue(JSStringRef attribute)
{
    if (JSStringIsEqualToUTF8CString(attribute, "AXHasTouchEventListener"))
        return [m_element _accessibilityHasTouchEventListener];
    if (JSStringIsEqualToUTF8CString(attribute, "AXIsStrongPasswordField"))
        return [m_element _accessibilityIsStrongPasswordField];
    return false;
}

bool AccessibilityUIElement::isAttributeSettable(JSStringRef attribute)
{
    return [m_element accessibilityIsAttributeSettable:[NSString stringWithJSStringRef:attribute]];
}

bool AccessibilityUIElement::isAttributeSupported(JSStringRef attribute)
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::parameterizedAttributeNames()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::role()
{
    return [[m_element _accessibilityWebRoleAsString] createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::subrole()
{
    return WTR::createJSString();
}

bool AccessibilityUIElement::scrollPageUp()
{
    return [m_element accessibilityScroll:UIAccessibilityScrollDirectionUp];
}

bool AccessibilityUIElement::scrollPageDown()
{
    return [m_element accessibilityScroll:UIAccessibilityScrollDirectionDown];
}
bool AccessibilityUIElement::scrollPageLeft()
{
    return [m_element accessibilityScroll:UIAccessibilityScrollDirectionLeft];
}

bool AccessibilityUIElement::scrollPageRight()
{
    return [m_element accessibilityScroll:UIAccessibilityScrollDirectionRight];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::roleDescription()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::computedRoleString()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::title()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::description()
{
    return concatenateAttributeAndValue(@"AXLabel", [m_element accessibilityLabel]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::liveRegionRelevant() const
{
    return [[m_element accessibilityARIARelevantStatus] createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::liveRegionStatus() const
{
    return [[m_element accessibilityARIALiveRegionStatus] createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::orientation() const
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringValue()
{
    return concatenateAttributeAndValue(@"AXValue", [m_element accessibilityValue]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::language()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::helpText() const
{
    return concatenateAttributeAndValue(@"AXHint", [m_element accessibilityHint]);
}

double AccessibilityUIElement::intValue() const
{
    return [[m_element accessibilityValue] integerValue];
}

double AccessibilityUIElement::minValue()
{
    return [m_element _accessibilityMinValue];
}

double AccessibilityUIElement::maxValue()
{
    return [m_element _accessibilityMaxValue];
}

void AccessibilityUIElement::setValue(JSStringRef valueText)
{
    [m_element _accessibilitySetValue:[NSString stringWithJSStringRef:valueText]];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::valueDescription()
{
    return WTR::createJSString();
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    return -1;
}

bool AccessibilityUIElement::isAtomicLiveRegion() const
{
    return [m_element accessibilityARIALiveRegionIsAtomic];
}

bool AccessibilityUIElement::isBusy() const
{
    return [m_element accessibilityARIAIsBusy];
}

bool AccessibilityUIElement::isEnabled()
{
    return false;
}

bool AccessibilityUIElement::isRequired() const
{
    return [m_element accessibilityIsRequired];
}

bool AccessibilityUIElement::isFocused() const
{
    return false;
}

bool AccessibilityUIElement::isSelected() const
{
    UIAccessibilityTraits traits = [m_element accessibilityTraits];
    bool result = (traits & [m_element _axSelectedTrait]) == [m_element _axSelectedTrait];
    return result;
}

bool AccessibilityUIElement::isExpanded() const
{
    return [m_element accessibilityIsExpanded];
}

bool AccessibilityUIElement::isChecked() const
{
    return false;
}

bool AccessibilityUIElement::isIndeterminate() const
{
    return false;
}

int AccessibilityUIElement::hierarchicalLevel() const
{
    return 0;
}

bool AccessibilityUIElement::ariaIsGrabbed() const
{
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::ariaDropEffects() const
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::embeddedImageDescription() const
{
    return [[m_element _accessibilityPhotoDescription] createJSStringRef];
}

int AccessibilityUIElement::lineForIndex(int index)
{
    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::boundsForRange(unsigned location, unsigned length)
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumnHeaders()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRowHeaders()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumns()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRows()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfVisibleCells()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfHeader()
{
    return WTR::createJSString();
}

int AccessibilityUIElement::rowCount()
{
    return [m_element accessibilityRowCount];
}

int AccessibilityUIElement::columnCount()
{
    return [m_element accessibilityColumnCount];
}

int AccessibilityUIElement::indexInTable()
{
    return -1;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rowIndexRange()
{
    NSRange range = [m_element accessibilityRowRange];
    NSMutableString* rangeDescription = [NSMutableString stringWithFormat:@"{%lu, %lu}", (unsigned long)range.location, (unsigned long)range.length];
    return [rangeDescription createJSStringRef];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::columnIndexRange()
{
    NSRange range = [m_element accessibilityColumnRange];
    NSMutableString* rangeDescription = [NSMutableString stringWithFormat:@"{%lu, %lu}", (unsigned long)range.location, (unsigned long)range.length];
    return [rangeDescription createJSStringRef];
}

AccessibilityUIElement AccessibilityUIElement::cellForColumnAndRow(unsigned col, unsigned row)
{
    return AccessibilityUIElement([m_element accessibilityElementForRow:row andColumn:col]);
}

void AccessibilityUIElement::scrollToMakeVisible()
{
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height)
{
}

void AccessibilityUIElement::scrollToGlobalPoint(int x, int y)
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectedTextRange()
{
    return WTR::createJSString();
}

void AccessibilityUIElement::assistiveTechnologySimulatedFocus()
{
    [m_element accessibilityElementDidBecomeFocused];
}

void AccessibilityUIElement::setSelectedTextRange(unsigned location, unsigned length)
{
}

void AccessibilityUIElement::increment()
{
    [m_element accessibilityIncrement];
}

void AccessibilityUIElement::decrement()
{
    [m_element accessibilityDecrement];
}

void AccessibilityUIElement::showMenu()
{
}

void AccessibilityUIElement::press()
{
    [m_element _accessibilityActivate];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::accessibilityValue() const
{
    return WTR::createJSString();
}

void AccessibilityUIElement::clearSelectedChildren() const
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentEncoding()
{
    return WTR::createJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentURI()
{
    return WTR::createJSString();
}

bool AccessibilityUIElement::addNotificationListener(JSObjectRef functionCallback)
{
    if (!functionCallback)
        return false;
    
    // iOS programmers should not be adding more than one notification listener per element.
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
    // iOS programmers should not be trying to remove a listener that's already removed.
    ASSERT(m_notificationHandler);
    
    [m_notificationHandler stopObserving];
    m_notificationHandler = nil;
}

bool AccessibilityUIElement::isFocusable() const
{
    return false;
}

bool AccessibilityUIElement::isSelectable() const
{
    return false;
}

bool AccessibilityUIElement::isMultiSelectable() const
{
    return false;
}

bool AccessibilityUIElement::isSelectedOptionActive() const
{
    return false;
}

bool AccessibilityUIElement::isVisible() const
{
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

bool AccessibilityUIElement::isIgnored() const
{
    return ![m_element isAccessibilityElement];
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
    return [m_element accessibilityHasPopup];
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::popupValue() const
{
    return [[m_element accessibilityPopupValue] createJSStringRef];
}

bool AccessibilityUIElement::hasDocumentRoleAncestor() const
{
    return [m_element accessibilityHasDocumentRoleAncestor];
}

bool AccessibilityUIElement::hasWebApplicationAncestor() const
{
    return [m_element accessibilityHasWebApplicationAncestor];
}

bool AccessibilityUIElement::isInDescriptionListDetail() const
{
    return [m_element accessibilityIsInDescriptionListDefinition];
}

bool AccessibilityUIElement::isInDescriptionListTerm() const
{
    return [m_element accessibilityIsInDescriptionListTerm];
}

bool AccessibilityUIElement::isInCell() const
{
    return [m_element _accessibilityIsInTableCell];
}

void AccessibilityUIElement::takeFocus()
{
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

unsigned AccessibilityUIElement::uiElementCountForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement *startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    NSDictionary *parameterizedAttribute = searchPredicateParameterizedAttributeForSearchCriteria(context, startElement, isDirectionNext, 5, searchKey, searchText, visibleOnly, immediateDescendantsOnly);
    id value = [m_element accessibilityFindMatchingObjects:parameterizedAttribute];
    if (![value isKindOfClass:[NSArray class]])
        return nullptr;
    for (id element in value) {
        if ([element isAccessibilityElement])
            return AccessibilityUIElement(element);
    }
    return AccessibilityUIElement([value firstObject]);
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectTextWithCriteria(JSContextRef context, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
{
    // FIXME: Implement.
    return nullptr;
}

double AccessibilityUIElement::numberAttributeValue(JSStringRef attribute)
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

JSRetainPtr<JSStringRef> AccessibilityUIElement::classList() const
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::domIdentifier() const
{
    return [[m_element accessibilityDOMIdentifier] createJSStringRef];
}

void AccessibilityUIElement::uiElementArrayAttributeValue(JSStringRef, Vector<AccessibilityUIElement>&) const
{
}

void AccessibilityUIElement::columnHeaders(Vector<AccessibilityUIElement>&) const
{
}

void AccessibilityUIElement::rowHeaders(Vector<AccessibilityUIElement>&) const
{
}

unsigned AccessibilityUIElement::selectedChildrenCount() const
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::selectedChildAtIndex(unsigned) const
{
    return 0;
}

bool AccessibilityUIElement::isInsertion()
{
    return [m_element accessibilityIsInsertion];
}

bool AccessibilityUIElement::isDeletion()
{
    return [m_element accessibilityIsDeletion];
}

bool AccessibilityUIElement::isFirstItemInSuggestion()
{
    return [m_element accessibilityIsFirstItemInSuggestion];
}

bool AccessibilityUIElement::isLastItemInSuggestion()
{
    return [m_element accessibilityIsLastItemInSuggestion];
}

bool AccessibilityUIElement::isMarkAnnotation() const
{
    return [m_element accessibilityIsMarkAnnotation];
}
