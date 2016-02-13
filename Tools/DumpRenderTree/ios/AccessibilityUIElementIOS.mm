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
#import "DumpRenderTree.h"
#import "AccessibilityUIElement.h"

#import "AccessibilityCommonMac.h"
#import "AccessibilityNotificationHandler.h"
#import <Foundation/Foundation.h>
#import <JavaScriptCore/JSRetainPtr.h>
#import <JavaScriptCore/JSStringRef.h>
#import <JavaScriptCore/JSStringRefCF.h>
#import <WebCore/TextGranularity.h>
#import <WebKit/WebFrame.h>
#import <WebKit/WebHTMLView.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

#import <UIKit/UIKit.h>

typedef void (*AXPostedNotificationCallback)(id element, NSString* notification, void* context);

AccessibilityUIElement::AccessibilityUIElement(PlatformUIElement element)
    : m_element(element)
    , m_notificationHandler(0)
{
    [m_element retain];
}

AccessibilityUIElement::AccessibilityUIElement(const AccessibilityUIElement& other)
    : m_element(other.m_element)
    , m_notificationHandler(0)

{
    [m_element retain];
}

AccessibilityUIElement::~AccessibilityUIElement()
{
    [m_element release];
}

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
- (NSArray *)elementsForRange:(NSRange)range;
- (NSString *)selectionRangeString;
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
- (UIAccessibilityTraits)_axContainedByFieldsetTrait;
- (id)_accessibilityFieldsetAncestor;

// TextMarker related
- (NSArray *)textMarkerRange;
- (NSInteger)lengthForTextMarkers:(NSArray *)textMarkers;
- (NSString *)stringForTextMarkers:(NSArray *)markers;
- (id)startOrEndTextMarkerForTextMarkers:(NSArray*)textMarkers isStart:(BOOL)isStart;
- (NSArray *)textMarkerRangeForMarkers:(NSArray *)textMarkers;
- (id)nextMarkerForMarker:(id)marker;
- (id)previousMarkerForMarker:(id)marker;
- (id)accessibilityObjectForTextMarker:(id)marker;
@end

@interface NSObject (WebAccessibilityObjectWrapperPrivate)
- (CGPathRef)_accessibilityPath;
@end

static JSStringRef concatenateAttributeAndValue(NSString* attribute, NSString* value)
{
    Vector<UniChar> buffer([attribute length]);
    [attribute getCharacters:buffer.data()];
    buffer.append(':');
    buffer.append(' ');
    
    Vector<UniChar> valueBuffer([value length]);
    [value getCharacters:valueBuffer.data()];
    buffer.appendVector(valueBuffer);
    
    return JSStringCreateWithCharacters(buffer.data(), buffer.size());
}

#pragma mark iPhone Attributes

JSStringRef AccessibilityUIElement::identifier()
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

JSStringRef AccessibilityUIElement::traits()
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


JSStringRef AccessibilityUIElement::url()
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
    // FIXME: implement
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
    [m_element accessibilityModifySelection:WebCore::CharacterGranularity increase:YES];
}

void AccessibilityUIElement::decreaseTextSelection()
{
    [m_element accessibilityModifySelection:WebCore::CharacterGranularity increase:NO];    
}

JSStringRef AccessibilityUIElement::stringForSelection() 
{ 
    NSString *stringForRange = [m_element selectionRangeString];
    if (!stringForRange)
        return 0;
    
    return [stringForRange createJSStringRef];
}

JSStringRef AccessibilityUIElement::stringForRange(unsigned location, unsigned length) 
{ 
    NSString *stringForRange = [m_element stringForRange:NSMakeRange(location, length)];
    if (!stringForRange)
        return 0;
    
    return [stringForRange createJSStringRef];
}

JSStringRef AccessibilityUIElement::attributedStringForRange(unsigned, unsigned)
{
    return JSStringCreateWithCharacters(0, 0);
}

bool AccessibilityUIElement::attributedStringRangeIsMisspelled(unsigned, unsigned)
{
    return false;
}


void AccessibilityUIElement::elementsForRange(unsigned location, unsigned length, Vector<AccessibilityUIElement>& elements)
{ 
    NSArray *elementsForRange = [m_element elementsForRange:NSMakeRange(location, length)];
    for (id object in elementsForRange) {
        AccessibilityUIElement element = AccessibilityUIElement(object);
        elements.append(element);
    }
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

JSStringRef AccessibilityUIElement::pathDescription() const
{
    NSMutableString *result = [NSMutableString stringWithString:@"\nStart Path\n"];
    CGPathRef pathRef = [m_element _accessibilityPath];
    
    CGPathApply(pathRef, result, _CGPathEnumerationIteration);
    
    return [result createJSStringRef];
}

#if SUPPORTS_AX_TEXTMARKERS && PLATFORM(IOS)

// Text markers

AccessibilityTextMarkerRange AccessibilityUIElement::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker*)
{
    return nullptr;
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

void AccessibilityUIElement::resetSelectedTextMarkerRange()
{
}

int AccessibilityUIElement::textMarkerRangeLength(AccessibilityTextMarkerRange* range)
{
    id textMarkers = (id)range->platformTextMarkerRange();
    return [m_element lengthForTextMarkers:textMarkers];
}

AccessibilityTextMarkerRange AccessibilityUIElement::textMarkerRangeForMarkers(AccessibilityTextMarker* startMarker, AccessibilityTextMarker* endMarker)
{
    NSArray *textMarkers = [NSArray arrayWithObjects:(id)startMarker->platformTextMarker(), (id)endMarker->platformTextMarker(), nil];
    id textMarkerRange = [m_element textMarkerRangeForMarkers:textMarkers];
    return AccessibilityTextMarkerRange(textMarkerRange);
}

AccessibilityTextMarker AccessibilityUIElement::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    id textMarkers = (id)range->platformTextMarkerRange();
    id textMarker = [m_element startOrEndTextMarkerForTextMarkers:textMarkers isStart:YES];
    return AccessibilityTextMarker(textMarker);
}

AccessibilityTextMarker AccessibilityUIElement::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange* range)
{
    id textMarkers = (id)range->platformTextMarkerRange();
    id textMarker = [m_element startOrEndTextMarkerForTextMarkers:textMarkers isStart:NO];
    return AccessibilityTextMarker(textMarker);
}

AccessibilityUIElement AccessibilityUIElement::accessibilityElementForTextMarker(AccessibilityTextMarker* marker)
{
    id obj = [m_element accessibilityObjectForTextMarker:(id)marker->platformTextMarker()];
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
    id previousMarker = [m_element previousMarkerForMarker:(id)textMarker->platformTextMarker()];
    return AccessibilityTextMarker(previousMarker);
}

AccessibilityTextMarker AccessibilityUIElement::nextTextMarker(AccessibilityTextMarker* textMarker)
{
    id nextMarker = [m_element nextMarkerForMarker:(id)textMarker->platformTextMarker()];
    return AccessibilityTextMarker(nextMarker);
}

JSStringRef AccessibilityUIElement::stringForTextMarkerRange(AccessibilityTextMarkerRange* markerRange)
{
    id textMarkers = (id)markerRange->platformTextMarkerRange();
    if (!textMarkers || ![textMarkers isKindOfClass:[NSArray class]])
        return JSStringCreateWithCharacters(0, 0);
    return [[m_element stringForTextMarkers:textMarkers] createJSStringRef];
}

bool AccessibilityUIElement::attributedStringForTextMarkerRangeContainsAttribute(JSStringRef, AccessibilityTextMarkerRange*)
{
    return false;
}

int AccessibilityUIElement::indexForTextMarker(AccessibilityTextMarker*)
{
    return -1;
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

bool AccessibilityUIElement::setSelectedVisibleTextRange(AccessibilityTextMarkerRange*)
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

#endif // SUPPORTS_AX_TEXTMARKERS && PLATFORM(IOS)

#pragma mark Unused

void AccessibilityUIElement::getLinkedUIElements(Vector<AccessibilityUIElement>& elementVector)
{
}

void AccessibilityUIElement::getDocumentLinks(Vector<AccessibilityUIElement>& elementVector)
{
}

JSStringRef AccessibilityUIElement::attributesOfLinkedUIElements()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfDocumentLinks()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfChildren()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::allAttributes()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::stringAttributeValue(JSStringRef attribute)
{
    if (JSStringIsEqualToUTF8CString(attribute, "AXPlaceholderValue"))
        return [[m_element accessibilityPlaceholderValue] createJSStringRef];
    
    if (JSStringIsEqualToUTF8CString(attribute, "AXARIACurrent"))
        return [[m_element accessibilityARIACurrentStatus] createJSStringRef];
    
    return JSStringCreateWithCharacters(0, 0);
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
    return false;
}

bool AccessibilityUIElement::isAttributeSettable(JSStringRef attribute)
{
    return false;
}

bool AccessibilityUIElement::isAttributeSupported(JSStringRef attribute)
{
    return false;
}

JSStringRef AccessibilityUIElement::parameterizedAttributeNames()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::role()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::subrole()
{
    return JSStringCreateWithCharacters(0, 0);
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

JSStringRef AccessibilityUIElement::roleDescription()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::computedRoleString()
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::title()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::description()
{
    return concatenateAttributeAndValue(@"AXLabel", [m_element accessibilityLabel]);
}

JSStringRef AccessibilityUIElement::orientation() const
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::stringValue()
{
    return concatenateAttributeAndValue(@"AXValue", [m_element accessibilityValue]);
}

JSStringRef AccessibilityUIElement::language()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::helpText() const
{
    return concatenateAttributeAndValue(@"AXHint", [m_element accessibilityHint]);
}

double AccessibilityUIElement::intValue() const
{
    return 0.0f;
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

JSStringRef AccessibilityUIElement::valueDescription()
{
    return JSStringCreateWithCharacters(0, 0);
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    return -1;
}

bool AccessibilityUIElement::isEnabled()
{
    return false;
}

bool AccessibilityUIElement::isRequired() const
{
    return false;
}

bool AccessibilityUIElement::isFocused() const
{
    // FIXME: implement
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
    return false;
}

bool AccessibilityUIElement::isChecked() const
{
    return false;
}

bool AccessibilityUIElement::isIndeterminate() const
{
    // FIXME: implement
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

JSStringRef AccessibilityUIElement::ariaDropEffects() const
{
    return JSStringCreateWithCharacters(0, 0);
}

int AccessibilityUIElement::lineForIndex(int index)
{
    return -1;
}

JSStringRef AccessibilityUIElement::boundsForRange(unsigned location, unsigned length)
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfColumnHeaders()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfRowHeaders()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfColumns()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfRows()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfVisibleCells()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfHeader()
{
    return JSStringCreateWithCharacters(0, 0);
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

JSStringRef AccessibilityUIElement::rowIndexRange()
{
    NSRange range = [m_element accessibilityRowRange];
    NSMutableString* rangeDescription = [NSMutableString stringWithFormat:@"{%lu, %lu}", (unsigned long)range.location, (unsigned long)range.length];
    return [rangeDescription createJSStringRef];
}

JSStringRef AccessibilityUIElement::columnIndexRange()
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
    // FIXME: implement
}

void AccessibilityUIElement::scrollToMakeVisibleWithSubFocus(int x, int y, int width, int height)
{
    // FIXME: implement
}

void AccessibilityUIElement::scrollToGlobalPoint(int x, int y)
{
    // FIXME: implement
}

JSStringRef AccessibilityUIElement::selectedTextRange()
{
    return JSStringCreateWithCharacters(0, 0);
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

JSStringRef AccessibilityUIElement::accessibilityValue() const
{
    // FIXME: implement
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::documentEncoding()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::documentURI()
{
    return JSStringCreateWithCharacters(0, 0);
}

bool AccessibilityUIElement::addNotificationListener(JSObjectRef functionCallback)
{
    if (!functionCallback)
        return false;
    
    // iOS programmers should not be adding more than one notification listener per element.
    // Other platforms may be different.
    if (m_notificationHandler)
        return false;
    m_notificationHandler = [[AccessibilityNotificationHandler alloc] init];
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
    [m_notificationHandler release];
    m_notificationHandler = nil;
}

bool AccessibilityUIElement::isFocusable() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isSelectable() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isMultiSelectable() const
{
    // FIXME: implement
    return false;
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
    return ![m_element isAccessibilityElement];
}

bool AccessibilityUIElement::hasPopup() const
{
    // FIXME: implement
    return false;
}

void AccessibilityUIElement::takeFocus()
{
    // FIXME: implement
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

unsigned AccessibilityUIElement::uiElementCountForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    // FIXME: implement
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    // FIXME: implement
    return 0;
}

JSStringRef AccessibilityUIElement::selectTextWithCriteria(JSContextRef context, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
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
    
    return 0;
}

JSStringRef AccessibilityUIElement::classList() const
{
    // FIXME: implement
    return nullptr;
}

void AccessibilityUIElement::uiElementArrayAttributeValue(JSStringRef, Vector<AccessibilityUIElement>&) const
{
    // FIXME: implement
}

void AccessibilityUIElement::columnHeaders(Vector<AccessibilityUIElement>&) const
{
    // FIXME: implement
}

void AccessibilityUIElement::rowHeaders(Vector<AccessibilityUIElement>&) const
{
    // FIXME: implement
}

unsigned AccessibilityUIElement::selectedChildrenCount() const
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::selectedChildAtIndex(unsigned) const
{
    return 0;
}
