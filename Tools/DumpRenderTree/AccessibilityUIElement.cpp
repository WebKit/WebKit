/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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

#include "config.h"

#if HAVE(ACCESSIBILITY)

#include "AccessibilityUIElement.h"

#include <JavaScriptCore/JSObjectRef.h>
#include <limits.h>

// Static Functions

static inline AccessibilityUIElement* toAXElement(JSObjectRef object)
{
    // FIXME: We should ASSERT that it is the right class here.
    return static_cast<AccessibilityUIElement*>(JSObjectGetPrivate(object));
}

static JSValueRef allAttributesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto attributes = toAXElement(thisObject)->allAttributes();
    return JSValueMakeString(context, attributes.get());
}

static JSValueRef attributesOfLinkedUIElementsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto linkedUIDescription = toAXElement(thisObject)->attributesOfLinkedUIElements();
    return JSValueMakeString(context, linkedUIDescription.get());
}

static JSValueRef attributesOfDocumentLinksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto linkedUIDescription = toAXElement(thisObject)->attributesOfDocumentLinks();
    return JSValueMakeString(context, linkedUIDescription.get());
}

static JSValueRef attributesOfChildrenCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto childrenDescription = toAXElement(thisObject)->attributesOfChildren();
    return JSValueMakeString(context, childrenDescription.get());
}

static JSValueRef parameterizedAttributeNamesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto parameterizedAttributeNames = toAXElement(thisObject)->parameterizedAttributeNames();
    return JSValueMakeString(context, parameterizedAttributeNames.get());
}

static JSValueRef attributesOfColumnHeadersCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto attributesOfColumnHeaders = toAXElement(thisObject)->attributesOfColumnHeaders();
    return JSValueMakeString(context, attributesOfColumnHeaders.get());
}

static JSValueRef attributesOfRowHeadersCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto attributesOfRowHeaders = toAXElement(thisObject)->attributesOfRowHeaders();
    return JSValueMakeString(context, attributesOfRowHeaders.get());
}

static JSValueRef attributesOfColumnsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto attributesOfColumns = toAXElement(thisObject)->attributesOfColumns();
    return JSValueMakeString(context, attributesOfColumns.get());
}

static JSValueRef attributesOfRowsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto attributesOfRows = toAXElement(thisObject)->attributesOfRows();
    return JSValueMakeString(context, attributesOfRows.get());
}

static JSValueRef attributesOfVisibleCellsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto attributesOfVisibleCells = toAXElement(thisObject)->attributesOfVisibleCells();
    return JSValueMakeString(context, attributesOfVisibleCells.get());
}

static JSValueRef attributesOfHeaderCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto attributesOfHeader = toAXElement(thisObject)->attributesOfHeader();
    return JSValueMakeString(context, attributesOfHeader.get());
}

static JSValueRef indexInTableCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->indexInTable());
}

static JSValueRef rowIndexRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto rowIndexRange = toAXElement(thisObject)->rowIndexRange();
    return JSValueMakeString(context, rowIndexRange.get());
}

static JSValueRef columnIndexRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto columnIndexRange = toAXElement(thisObject)->columnIndexRange();
    return JSValueMakeString(context, columnIndexRange.get());
}

static JSValueRef lineForIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = -1;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);
    
    return JSValueMakeNumber(context, toAXElement(thisObject)->lineForIndex(indexNumber));
}

static JSValueRef rangeForLineCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = -1;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);
    
    auto rangeLine = toAXElement(thisObject)->rangeForLine(indexNumber);
    return JSValueMakeString(context, rangeLine.get());
}

static JSValueRef boundsForRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    unsigned location = UINT_MAX, length = 0;
    if (argumentCount == 2) {
        location = JSValueToNumber(context, arguments[0], exception);
        length = JSValueToNumber(context, arguments[1], exception);
    }

    auto boundsDescription = toAXElement(thisObject)->boundsForRange(location, length);
    return JSValueMakeString(context, boundsDescription.get());    
}

static JSValueRef rangeForPositionCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int x = 0, y = 0;
    if (argumentCount == 2) {
        x = JSValueToNumber(context, arguments[0], exception);
        y = JSValueToNumber(context, arguments[1], exception);
    }
    
    auto rangeDescription = toAXElement(thisObject)->rangeForPosition(x, y);
    return JSValueMakeString(context, rangeDescription.get());    
}

static JSValueRef stringForRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    unsigned location = UINT_MAX, length = 0;
    if (argumentCount == 2) {
        location = JSValueToNumber(context, arguments[0], exception);
        length = JSValueToNumber(context, arguments[1], exception);
    }
    
    auto stringDescription = toAXElement(thisObject)->stringForRange(location, length);
    return JSValueMakeString(context, stringDescription.get());    
}

static JSValueRef attributedStringForRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    unsigned location = UINT_MAX, length = 0;
    if (argumentCount == 2) {
        location = JSValueToNumber(context, arguments[0], exception);
        length = JSValueToNumber(context, arguments[1], exception);
    }
    
    auto stringDescription = toAXElement(thisObject)->attributedStringForRange(location, length);
    return JSValueMakeString(context, stringDescription.get());    
}

static JSValueRef attributedStringRangeIsMisspelledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    unsigned location = UINT_MAX, length = 0;
    if (argumentCount == 2) {
        location = JSValueToNumber(context, arguments[0], exception);
        length = JSValueToNumber(context, arguments[1], exception);
    }
    
    return JSValueMakeBoolean(context, toAXElement(thisObject)->attributedStringRangeIsMisspelled(location, length));
}

static JSValueRef uiElementCountForSearchPredicateCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityUIElement* startElement = nullptr;
    bool isDirectionNext = true;
    JSValueRef searchKey = nullptr;
    JSRetainPtr<JSStringRef> searchText = nullptr;
    bool visibleOnly = false;
    bool immediateDescendantsOnly = false;
    if (argumentCount >= 5 && argumentCount <= 6) {
        if (JSValueIsObject(context, arguments[0]))
            startElement = toAXElement(JSValueToObject(context, arguments[0], exception));
        
        isDirectionNext = JSValueToBoolean(context, arguments[1]);
        
        searchKey = arguments[2];
        
        if (JSValueIsString(context, arguments[3]))
            searchText = adopt(JSValueToStringCopy(context, arguments[3], exception));
        
        visibleOnly = JSValueToBoolean(context, arguments[4]);
        
        if (argumentCount == 6)
            immediateDescendantsOnly = JSValueToBoolean(context, arguments[5]);
    }
    
    return JSValueMakeNumber(context, toAXElement(thisObject)->uiElementCountForSearchPredicate(context, startElement, isDirectionNext, searchKey, searchText.get(), visibleOnly, immediateDescendantsOnly));
}

static JSValueRef uiElementForSearchPredicateCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityUIElement* startElement = nullptr;
    bool isDirectionNext = true;
    JSValueRef searchKey = nullptr;
    JSRetainPtr<JSStringRef> searchText = nullptr;
    bool visibleOnly = false;
    bool immediateDescendantsOnly = false;
    if (argumentCount >= 5 && argumentCount <= 6) {
        if (JSValueIsObject(context, arguments[0]))
            startElement = toAXElement(JSValueToObject(context, arguments[0], exception));
        
        isDirectionNext = JSValueToBoolean(context, arguments[1]);
        
        searchKey = arguments[2];
        
        if (JSValueIsString(context, arguments[3]))
            searchText = adopt(JSValueToStringCopy(context, arguments[3], exception));
        
        visibleOnly = JSValueToBoolean(context, arguments[4]);
        
        if (argumentCount == 6)
            immediateDescendantsOnly = JSValueToBoolean(context, arguments[5]);
    }
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->uiElementForSearchPredicate(context, startElement, isDirectionNext, searchKey, searchText.get(), visibleOnly, immediateDescendantsOnly));
}

static JSValueRef selectTextWithCriteriaCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 2 || argumentCount > 4)
        return JSValueMakeUndefined(context);
    
    auto ambiguityResolution = adopt(JSValueToStringCopy(context, arguments[0], exception));
    JSValueRef searchStrings = arguments[1];
    JSStringRef replacementString = nullptr;
    if (argumentCount == 3)
        replacementString = JSValueToStringCopy(context, arguments[2], exception);
    JSStringRef activityString = nullptr;
    if (argumentCount == 4)
        activityString = JSValueToStringCopy(context, arguments[3], exception);
    
    auto result = toAXElement(thisObject)->selectTextWithCriteria(context, ambiguityResolution.get(), searchStrings, replacementString, activityString);
    if (replacementString)
        JSStringRelease(replacementString);
    if (activityString)
        JSStringRelease(activityString);
    return JSValueMakeString(context, result.get());
}

#if PLATFORM(MAC)
static JSValueRef searchTextWithCriteriaCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSValueRef searchStrings = arguments[0];
    JSRetainPtr<JSStringRef> startFrom;
    if (argumentCount > 1)
        startFrom = adopt(JSValueToStringCopy(context, arguments[1], exception));
    JSRetainPtr<JSStringRef> direction;
    if (argumentCount > 2)
        direction = adopt(JSValueToStringCopy(context, arguments[2], exception));

    return toAXElement(thisObject)->searchTextWithCriteria(context, searchStrings, startFrom.get(), direction.get());
}
#endif

static JSValueRef indexOfChildCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 1)
        return 0;
    
    JSObjectRef otherElement = JSValueToObject(context, arguments[0], exception);
    AccessibilityUIElement* childElement = toAXElement(otherElement);
    return JSValueMakeNumber(context, (double)toAXElement(thisObject)->indexOfChild(childElement));
}

#if PLATFORM(IOS_FAMILY)

static JSValueRef headerElementAtIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 1)
        return 0;
    
    unsigned index = JSValueToNumber(context, arguments[0], exception);
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->headerElementAtIndex(index));
}

static JSValueRef linkedElementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->linkedElement());
}

static JSValueRef elementsForRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 2)
        return 0;
    
    unsigned location = JSValueToNumber(context, arguments[0], exception);
    unsigned length = JSValueToNumber(context, arguments[1], exception);
    
    Vector<AccessibilityUIElement> elements;
    toAXElement(thisObject)->elementsForRange(location, length, elements);
    
    JSValueRef arrayResult = JSObjectMakeArray(context, 0, 0, 0);
    JSObjectRef arrayObj = JSValueToObject(context, arrayResult, 0);
    unsigned elementsSize = elements.size();
    for (unsigned k = 0; k < elementsSize; ++k)
        JSObjectSetPropertyAtIndex(context, arrayObj, k, AccessibilityUIElement::makeJSAccessibilityUIElement(context, elements[k]), 0);
    
    return arrayResult;
}

static JSValueRef increaseTextSelectionCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->increaseTextSelection();
    return JSValueMakeUndefined(context);
}

static JSValueRef scrollPageUpCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->scrollPageUp());
}

static JSValueRef scrollPageDownCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->scrollPageDown());
}

static JSValueRef scrollPageLeftCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->scrollPageLeft());
}

static JSValueRef scrollPageRightCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->scrollPageRight());
}

static JSValueRef decreaseTextSelectionCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->decreaseTextSelection();
    return JSValueMakeUndefined(context);
}

static JSValueRef assistiveTechnologySimulatedFocusCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->assistiveTechnologySimulatedFocus();
    return JSValueMakeUndefined(context);
}

static JSValueRef fieldsetAncestorElementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->fieldsetAncestorElement());
}

static JSValueRef attributedStringForElementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    auto stringDescription = toAXElement(thisObject)->attributedStringForElement();
    return JSValueMakeString(context, stringDescription.get());
}

#endif

static JSValueRef childAtIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = -1;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->getChildAtIndex(indexNumber));
}

static JSValueRef selectedChildAtIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = -1;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->selectedChildAtIndex(indexNumber));
}

static JSValueRef linkedUIElementAtIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = -1;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->linkedUIElementAtIndex(indexNumber));
}

static JSValueRef disclosedRowAtIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = 0;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->disclosedRowAtIndex(indexNumber));
}

static JSValueRef ariaOwnsElementAtIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = 0;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->ariaOwnsElementAtIndex(indexNumber));
}

static JSValueRef ariaFlowToElementAtIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = 0;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->ariaFlowToElementAtIndex(indexNumber));
}

static JSValueRef ariaControlsElementAtIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = 0;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);

    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->ariaControlsElementAtIndex(indexNumber));
}

static JSValueRef selectedRowAtIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = 0;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->selectedRowAtIndex(indexNumber));
}

static JSValueRef rowAtIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = 0;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->rowAtIndex(indexNumber));
}

static JSValueRef isEqualCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSObjectRef otherElement = 0;
    if (argumentCount == 1)
        otherElement = JSValueToObject(context, arguments[0], exception);
    else
        return JSValueMakeBoolean(context, false);
    
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isEqual(toAXElement(otherElement)));
}

static JSValueRef setValueCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> valueText = 0;
    if (argumentCount == 1) {
        if (JSValueIsString(context, arguments[0]))
            valueText = adopt(JSValueToStringCopy(context, arguments[0], exception));
    }
    
    toAXElement(thisObject)->setValue(valueText.get());
    
    return JSValueMakeUndefined(context);
}

static JSValueRef setSelectedChildCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSObjectRef element = 0;
    if (argumentCount == 1)
        element = JSValueToObject(context, arguments[0], exception);

    toAXElement(thisObject)->setSelectedChild(toAXElement(element));

    return JSValueMakeUndefined(context);
}

static JSValueRef setSelectedChildAtIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount == 1) {
        unsigned indexNumber = JSValueToNumber(context, arguments[0], exception);
        toAXElement(thisObject)->setSelectedChildAtIndex(indexNumber);
    }
    return JSValueMakeUndefined(context);
}

static JSValueRef removeSelectionAtIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount == 1) {
        unsigned indexNumber = JSValueToNumber(context, arguments[0], exception);
        toAXElement(thisObject)->removeSelectionAtIndex(indexNumber);
    }
    return JSValueMakeUndefined(context);
}

static JSValueRef elementAtPointCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int x = 0;
    int y = 0;
    if (argumentCount == 2) {
        x = JSValueToNumber(context, arguments[0], exception);
        y = JSValueToNumber(context, arguments[1], exception);
    }
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->elementAtPoint(x, y));
}

static JSValueRef isAttributeSupportedCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSStringRef attribute = 0;
    if (argumentCount == 1)
        attribute = JSValueToStringCopy(context, arguments[0], exception);    
    JSValueRef result = JSValueMakeBoolean(context, toAXElement(thisObject)->isAttributeSupported(attribute));
    if (attribute)
        JSStringRelease(attribute);
    return result;
}

static JSValueRef isAttributeSettableCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSStringRef attribute = 0;
    if (argumentCount == 1)
        attribute = JSValueToStringCopy(context, arguments[0], exception);    
    JSValueRef result = JSValueMakeBoolean(context, toAXElement(thisObject)->isAttributeSettable(attribute));
    if (attribute)
        JSStringRelease(attribute);
    return result;
}

static JSValueRef isPressActionSupportedCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isPressActionSupported());
}

static JSValueRef isIncrementActionSupportedCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isIncrementActionSupported());
}

static JSValueRef isDecrementActionSupportedCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isDecrementActionSupported());
}

static JSValueRef boolAttributeValueCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSStringRef attribute = 0;
    if (argumentCount == 1)
        attribute = JSValueToStringCopy(context, arguments[0], exception);
    bool val = toAXElement(thisObject)->boolAttributeValue(attribute);
    JSValueRef result = JSValueMakeBoolean(context, val);
    if (attribute)
        JSStringRelease(attribute);
    return result;
}

static JSValueRef setBoolAttributeValueCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSStringRef attribute = nullptr;
    bool value = false;
    if (argumentCount == 2) {
        attribute = JSValueToStringCopy(context, arguments[0], exception);
        value = JSValueToBoolean(context, arguments[1]);
    }
    toAXElement(thisObject)->setBoolAttributeValue(attribute, value);
    if (attribute)
        JSStringRelease(attribute);
    return JSValueMakeUndefined(context);
}

static JSValueRef stringAttributeValueCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSStringRef attribute = 0;
    if (argumentCount == 1)
        attribute = JSValueToStringCopy(context, arguments[0], exception);
    auto stringAttributeValue = toAXElement(thisObject)->stringAttributeValue(attribute);
    JSValueRef result = JSValueMakeString(context, stringAttributeValue.get());
    if (attribute)
        JSStringRelease(attribute);
    return result;
}

static JSValueRef convertElementsToObjectArray(JSContextRef context, Vector<AccessibilityUIElement>& elements, JSValueRef* exception)
{
    JSValueRef arrayResult = JSObjectMakeArray(context, 0, 0, 0);
    JSObjectRef arrayObj = JSValueToObject(context, arrayResult, 0);

    size_t elementCount = elements.size();
    for (size_t i = 0; i < elementCount; ++i)
        JSObjectSetPropertyAtIndex(context, arrayObj, i, AccessibilityUIElement::makeJSAccessibilityUIElement(context, elements[i]), 0);

    return arrayResult;
}

static JSValueRef columnHeadersCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    Vector<AccessibilityUIElement> elements;
    toAXElement(thisObject)->columnHeaders(elements);
    return convertElementsToObjectArray(context, elements, exception);
}

static JSValueRef rowHeadersCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    Vector<AccessibilityUIElement> elements;
    toAXElement(thisObject)->rowHeaders(elements);
    return convertElementsToObjectArray(context, elements, exception);
}

static JSValueRef uiElementArrayAttributeValueCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 1)
        return JSValueMakeUndefined(context);
    
    auto attribute = adopt(JSValueToStringCopy(context, arguments[0], exception));
    
    Vector<AccessibilityUIElement> elements;
    toAXElement(thisObject)->uiElementArrayAttributeValue(attribute.get(), elements);
    return convertElementsToObjectArray(context, elements, exception);
}

static JSValueRef uiElementAttributeValueCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> attribute;
    if (argumentCount == 1)
        attribute = adopt(JSValueToStringCopy(context, arguments[0], exception));
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->uiElementAttributeValue(attribute.get()));
}

static JSValueRef numberAttributeValueCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSStringRef attribute = 0;
    if (argumentCount == 1)
        attribute = JSValueToStringCopy(context, arguments[0], exception);
    double val = toAXElement(thisObject)->numberAttributeValue(attribute);
    JSValueRef result = JSValueMakeNumber(context, val);
    if (attribute)
        JSStringRelease(attribute);
    return result;
}

static JSValueRef cellForColumnAndRowCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    unsigned column = 0, row = 0;
    if (argumentCount == 2) {
        column = JSValueToNumber(context, arguments[0], exception);
        row = JSValueToNumber(context, arguments[1], exception);
    }
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->cellForColumnAndRow(column, row));
}

static JSValueRef titleUIElementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->titleUIElement());
}

static JSValueRef parentElementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->parentElement());
}

static JSValueRef disclosedByRowCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->disclosedByRow());
}

static JSValueRef setSelectedTextRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    unsigned location = UINT_MAX, length = 0;
    if (argumentCount == 2) {
        location = JSValueToNumber(context, arguments[0], exception);
        length = JSValueToNumber(context, arguments[1], exception);
    }
    
    toAXElement(thisObject)->setSelectedTextRange(location, length);
    return JSValueMakeUndefined(context);
}

static JSValueRef incrementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->increment();
    return JSValueMakeUndefined(context);
}

static JSValueRef decrementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->decrement();
    return JSValueMakeUndefined(context);
}

static JSValueRef showMenuCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->showMenu();
    return JSValueMakeUndefined(context);
}

static JSValueRef pressCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->press();
    return JSValueMakeUndefined(context);
}

static JSValueRef scrollToMakeVisibleWithSubFocusCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    unsigned x = 0;
    unsigned y = 0;
    unsigned width = 0;
    unsigned height = 0;
    if (argumentCount == 4) {
        x = JSValueToNumber(context, arguments[0], exception);
        y = JSValueToNumber(context, arguments[1], exception);
        width = JSValueToNumber(context, arguments[2], exception);
        height = JSValueToNumber(context, arguments[3], exception);
    }

    toAXElement(thisObject)->scrollToMakeVisibleWithSubFocus(x, y, width, height);
    return JSValueMakeUndefined(context);
}

static JSValueRef scrollToGlobalPointCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    unsigned x = 0;
    unsigned y = 0;
    if (argumentCount == 2) {
        x = JSValueToNumber(context, arguments[0], exception);
        y = JSValueToNumber(context, arguments[1], exception);
    }

    toAXElement(thisObject)->scrollToGlobalPoint(x, y);
    return JSValueMakeUndefined(context);
}

static JSValueRef scrollToMakeVisibleCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->scrollToMakeVisible();
    return JSValueMakeUndefined(context);
}

static JSValueRef takeFocusCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->takeFocus();
    return JSValueMakeUndefined(context);
}

static JSValueRef takeSelectionCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->takeSelection();
    return JSValueMakeUndefined(context);
}

static JSValueRef addSelectionCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->addSelection();
    return JSValueMakeUndefined(context);
}

static JSValueRef removeSelectionCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->removeSelection();
    return JSValueMakeUndefined(context);
}

static JSValueRef lineTextMarkerRangeForTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* textMarker = nullptr;
    if (argumentCount == 1)
        textMarker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    return AccessibilityTextMarkerRange::makeJSAccessibilityTextMarkerRange(context, toAXElement(thisObject)->lineTextMarkerRangeForTextMarker(textMarker));
}

static JSValueRef textMarkerRangeForElementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityUIElement* uiElement = 0;
    if (argumentCount == 1)
        uiElement = toAXElement(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarkerRange::makeJSAccessibilityTextMarkerRange(context, toAXElement(thisObject)->textMarkerRangeForElement(uiElement));
}

static JSValueRef selectedTextMarkerRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return AccessibilityTextMarkerRange::makeJSAccessibilityTextMarkerRange(context, toAXElement(thisObject)->selectedTextMarkerRange());
}

static JSValueRef resetSelectedTextMarkerRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->resetSelectedTextMarkerRange();
    return JSValueMakeUndefined(context);
}

static JSValueRef replaceTextInRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 3)
        return JSValueMakeUndefined(context);

    auto text = adopt(JSValueToStringCopy(context, arguments[0], exception));
    int position = JSValueToNumber(context, arguments[1], exception);
    int length = JSValueToNumber(context, arguments[2], exception);

    return JSValueMakeBoolean(context, toAXElement(thisObject)->replaceTextInRange(text.get(), position, length));
}

static JSValueRef insertTextCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (!argumentCount)
        return JSValueMakeUndefined(context);

    auto text = adopt(JSValueToStringCopy(context, arguments[0], exception));
    return JSValueMakeBoolean(context, toAXElement(thisObject)->insertText(text.get()));
}

static JSValueRef attributedStringForTextMarkerRangeContainsAttributeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarkerRange* markerRange = 0;
    JSStringRef attribute = 0;
    if (argumentCount == 2) {
        attribute = JSValueToStringCopy(context, arguments[0], exception);
        markerRange = toTextMarkerRange(JSValueToObject(context, arguments[1], exception));
    }
    
    JSValueRef result = JSValueMakeBoolean(context, toAXElement(thisObject)->attributedStringForTextMarkerRangeContainsAttribute(attribute, markerRange));
    if (attribute)
        JSStringRelease(attribute);
    
    return result;    
}

static JSValueRef indexForTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = 0;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return JSValueMakeNumber(context, toAXElement(thisObject)->indexForTextMarker(marker));
}

static JSValueRef isTextMarkerValidCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = 0;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isTextMarkerValid(marker));
}

static JSValueRef textMarkerForIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int textIndex = 0;
    if (argumentCount == 1)
        textIndex = JSValueToNumber(context, arguments[0], exception);
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->textMarkerForIndex(textIndex));
}

static JSValueRef textMarkerRangeLengthCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarkerRange* range = 0;
    if (argumentCount == 1)
        range = toTextMarkerRange(JSValueToObject(context, arguments[0], exception));
    
    return JSValueMakeNumber(context, (int)toAXElement(thisObject)->textMarkerRangeLength(range));
}

static JSValueRef nextTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = 0;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->nextTextMarker(marker));
}

static JSValueRef previousTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = 0;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->previousTextMarker(marker));
}

static JSValueRef stringForTextMarkerRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarkerRange* markerRange = 0;
    if (argumentCount == 1)
        markerRange = toTextMarkerRange(JSValueToObject(context, arguments[0], exception));
    
    auto markerRangeString = toAXElement(thisObject)->stringForTextMarkerRange(markerRange);
    return JSValueMakeString(context, markerRangeString.get());    
}

static JSValueRef attributedStringForTextMarkerRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarkerRange* markerRange = 0;
    if (argumentCount == 1)
        markerRange = toTextMarkerRange(JSValueToObject(context, arguments[0], exception));

    auto markerRangeString = toAXElement(thisObject)->attributedStringForTextMarkerRange(markerRange);
    return JSValueMakeString(context, markerRangeString.get());
}

static JSValueRef attributedStringForTextMarkerRangeWithOptionsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarkerRange* markerRange = nullptr;
    bool includeSpellCheck = false;
    if (argumentCount == 2) {
        markerRange = toTextMarkerRange(JSValueToObject(context, arguments[0], exception));
        includeSpellCheck = JSValueToBoolean(context, arguments[1]);
    }

    auto markerRangeString = toAXElement(thisObject)->attributedStringForTextMarkerRangeWithOptions(markerRange, includeSpellCheck);
    return JSValueMakeString(context, markerRangeString.get());
}

static JSValueRef endTextMarkerForBoundsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    if (argumentCount == 4) {
        x = JSValueToNumber(context, arguments[0], exception);
        y = JSValueToNumber(context, arguments[1], exception);
        width = JSValueToNumber(context, arguments[2], exception);
        height = JSValueToNumber(context, arguments[3], exception);
    }
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->endTextMarkerForBounds(x, y, width, height));
}

static JSValueRef startTextMarkerForBoundsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    if (argumentCount == 4) {
        x = JSValueToNumber(context, arguments[0], exception);
        y = JSValueToNumber(context, arguments[1], exception);
        width = JSValueToNumber(context, arguments[2], exception);
        height = JSValueToNumber(context, arguments[3], exception);
    }
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->startTextMarkerForBounds(x, y, width, height));
}

static JSValueRef textMarkerForPointCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int x = 0;
    int y = 0;
    if (argumentCount == 2) {
        x = JSValueToNumber(context, arguments[0], exception);
        y = JSValueToNumber(context, arguments[1], exception);
    }
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->textMarkerForPoint(x, y));
}

static JSValueRef textMarkerRangeForMarkersCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* startMarker = nullptr;
    AccessibilityTextMarker* endMarker = nullptr;
    if (argumentCount == 2) {
        startMarker = toTextMarker(JSValueToObject(context, arguments[0], exception));
        endMarker = toTextMarker(JSValueToObject(context, arguments[1], exception));
    }
    
    return AccessibilityTextMarkerRange::makeJSAccessibilityTextMarkerRange(context, toAXElement(thisObject)->textMarkerRangeForMarkers(startMarker, endMarker));
}

static JSValueRef startTextMarkerForTextMarkerRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarkerRange* markerRange = 0;
    if (argumentCount == 1)
        markerRange = toTextMarkerRange(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->startTextMarkerForTextMarkerRange(markerRange));
}

static JSValueRef endTextMarkerForTextMarkerRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarkerRange* markerRange = 0;
    if (argumentCount == 1)
        markerRange = toTextMarkerRange(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->endTextMarkerForTextMarkerRange(markerRange));
}

static JSValueRef accessibilityElementForTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = nullptr;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->accessibilityElementForTextMarker(marker));
}

static JSValueRef startTextMarkerCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->startTextMarker());
}

static JSValueRef endTextMarkerCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->endTextMarker());
}

static JSValueRef leftWordTextMarkerRangeForTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = nullptr;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarkerRange::makeJSAccessibilityTextMarkerRange(context, toAXElement(thisObject)->leftWordTextMarkerRangeForTextMarker(marker));
}

static JSValueRef rightWordTextMarkerRangeForTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = nullptr;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarkerRange::makeJSAccessibilityTextMarkerRange(context, toAXElement(thisObject)->rightWordTextMarkerRangeForTextMarker(marker));
}

static JSValueRef previousWordStartTextMarkerForTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = nullptr;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->previousWordStartTextMarkerForTextMarker(marker));
}

static JSValueRef nextWordEndTextMarkerForTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = nullptr;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->nextWordEndTextMarkerForTextMarker(marker));
}

static JSValueRef paragraphTextMarkerRangeForTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = nullptr;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarkerRange::makeJSAccessibilityTextMarkerRange(context, toAXElement(thisObject)->paragraphTextMarkerRangeForTextMarker(marker));
}

static JSValueRef previousParagraphStartTextMarkerForTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = nullptr;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->previousParagraphStartTextMarkerForTextMarker(marker));
}

static JSValueRef nextParagraphEndTextMarkerForTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = nullptr;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->nextParagraphEndTextMarkerForTextMarker(marker));
}

static JSValueRef sentenceTextMarkerRangeForTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = nullptr;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarkerRange::makeJSAccessibilityTextMarkerRange(context, toAXElement(thisObject)->sentenceTextMarkerRangeForTextMarker(marker));
}

static JSValueRef previousSentenceStartTextMarkerForTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = nullptr;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->previousSentenceStartTextMarkerForTextMarker(marker));
}

static JSValueRef nextSentenceEndTextMarkerForTextMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityTextMarker* marker = nullptr;
    if (argumentCount == 1)
        marker = toTextMarker(JSValueToObject(context, arguments[0], exception));
    
    return AccessibilityTextMarker::makeJSAccessibilityTextMarker(context, toAXElement(thisObject)->nextSentenceEndTextMarkerForTextMarker(marker));
}

static JSValueRef setSelectedVisibleTextRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    AccessibilityUIElement* uiElement = toAXElement(thisObject);
    AccessibilityTextMarkerRange* textMarkerRange = nullptr;
    if (argumentCount == 1)
        textMarkerRange = toTextMarkerRange(JSValueToObject(context, arguments[0], exception));

    if (uiElement)
        return JSValueMakeBoolean(context, uiElement->setSelectedVisibleTextRange(textMarkerRange));

    return JSValueMakeBoolean(context, false);
}

// Static Value Getters

static JSValueRef getARIADropEffectsCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto dropEffects = toAXElement(thisObject)->ariaDropEffects();
    return JSValueMakeString(context, dropEffects.get());
}

static JSValueRef getClassListCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto classList = toAXElement(thisObject)->classList();
    return JSValueMakeString(context, classList.get());
}

static JSValueRef getARIAIsGrabbedCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->ariaIsGrabbed());
}

static JSValueRef getIsValidCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    AccessibilityUIElement* uiElement = toAXElement(thisObject);
    if (!uiElement->platformUIElement())
        return JSValueMakeBoolean(context, false);
    
    // There might be other platform logic that one could check here...
    
    return JSValueMakeBoolean(context, true);
}

static JSValueRef getRoleCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto role = toAXElement(thisObject)->role();
    return JSValueMakeString(context, role.get());
}

static JSValueRef getSubroleCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto role = toAXElement(thisObject)->subrole();
    return JSValueMakeString(context, role.get());
}

static JSValueRef getRoleDescriptionCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto roleDesc = toAXElement(thisObject)->roleDescription();
    return JSValueMakeString(context, roleDesc.get());
}

static JSValueRef getComputedRoleStringCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto compRole = toAXElement(thisObject)->computedRoleString();
    return JSValueMakeString(context, compRole.get());
}

static JSValueRef getTitleCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto title = toAXElement(thisObject)->title();
    return JSValueMakeString(context, title.get());
}

static JSValueRef getDescriptionCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto description = toAXElement(thisObject)->description();
    return JSValueMakeString(context, description.get());
}

static JSValueRef getStringValueCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto stringValue = toAXElement(thisObject)->stringValue();
    return JSValueMakeString(context, stringValue.get());
}

static JSValueRef getLanguageCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto language = toAXElement(thisObject)->language();
    return JSValueMakeString(context, language.get());
}

static JSValueRef getHelpTextCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto language = toAXElement(thisObject)->helpText();
    return JSValueMakeString(context, language.get());
}

static JSValueRef getOrientationCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto orientation = toAXElement(thisObject)->orientation();
    return JSValueMakeString(context, orientation.get());
}

static JSValueRef getChildrenCountCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->childrenCount());
}

static JSValueRef rowCountCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->rowCount());
}

static JSValueRef columnCountCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->columnCount());
}

static JSValueRef getXCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->x());
}

static JSValueRef getYCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->y());
}

static JSValueRef getWidthCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->width());
}

static JSValueRef getHeightCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->height());
}

static JSValueRef getClickPointXCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->clickPointX());
}

static JSValueRef getClickPointYCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->clickPointY());
}

static JSValueRef getIntValueCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->intValue());
}

static JSValueRef getMinValueCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->minValue());
}

static JSValueRef getMaxValueCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->maxValue());
}

static JSValueRef getInsertionPointLineNumberCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->insertionPointLineNumber());
}

static JSValueRef getPathDescriptionCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto pathDescription = toAXElement(thisObject)->pathDescription();
    return JSValueMakeString(context, pathDescription.get());
}

static JSValueRef getSelectedTextRangeCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto selectedTextRange = toAXElement(thisObject)->selectedTextRange();
    return JSValueMakeString(context, selectedTextRange.get());
}

static JSValueRef getIsEnabledCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isEnabled());
}

static JSValueRef getIsRequiredCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isRequired());
}

static JSValueRef getIsFocusedCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isFocused());
}

static JSValueRef getIsFocusableCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isFocusable());
}

static JSValueRef getIsSelectedCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isSelected());
}

static JSValueRef getIsSelectableCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isSelectable());
}

static JSValueRef getIsMultiSelectableCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isMultiSelectable());
}

static JSValueRef getIsSelectedOptionActiveCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isSelectedOptionActive());
}

static JSValueRef getIsExpandedCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isExpanded());
}

static JSValueRef getIsCheckedCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isChecked());
}

static JSValueRef getIsIndeterminate(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isIndeterminate());
}

static JSValueRef getIsVisibleCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isVisible());
}

static JSValueRef getIsOffScreenCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isOffScreen());
}

static JSValueRef getIsCollapsedCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isCollapsed());
}

static JSValueRef isIgnoredCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isIgnored());
}

static JSValueRef speakAsCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    auto speakAsString = toAXElement(thisObject)->speakAs();
    return JSValueMakeString(context, speakAsString.get());
}

static JSValueRef selectedChildrenCountCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->selectedChildrenCount());
}

static JSValueRef horizontalScrollbarCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->horizontalScrollbar());
}

static JSValueRef verticalScrollbarCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->verticalScrollbar());
}

static JSValueRef getHasPopupCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->hasPopup());
}

static JSValueRef getPopupValueCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeString(context, toAXElement(thisObject)->popupValue().get());
}

static JSValueRef hierarchicalLevelCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->hierarchicalLevel());
}

static JSValueRef getValueDescriptionCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto valueDescription = toAXElement(thisObject)->valueDescription();
    return JSValueMakeString(context, valueDescription.get());
}

static JSValueRef getAccessibilityValueCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto accessibilityValue = toAXElement(thisObject)->accessibilityValue();
    return JSValueMakeString(context, accessibilityValue.get());
}

static JSValueRef getDocumentEncodingCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto documentEncoding = toAXElement(thisObject)->documentEncoding();
    return JSValueMakeString(context, documentEncoding.get());
}

static JSValueRef getDocumentURICallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto documentURI = toAXElement(thisObject)->documentURI();
    return JSValueMakeString(context, documentURI.get());
}

static JSValueRef getURLCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto url = toAXElement(thisObject)->url();
    return JSValueMakeString(context, url.get());
}

static JSValueRef addNotificationListenerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 1)
        return JSValueMakeBoolean(context, false);
    
    JSObjectRef callback = JSValueToObject(context, arguments[0], exception);
    bool succeeded = toAXElement(thisObject)->addNotificationListener(callback);
    return JSValueMakeBoolean(context, succeeded);
}

static JSValueRef removeNotificationListenerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->removeNotificationListener();
    return JSValueMakeUndefined(context);
}

#if PLATFORM(GTK)
static JSValueRef characterAtOffsetCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int offset = -1;
    if (argumentCount == 1)
        offset = JSValueToNumber(context, arguments[0], exception);

    auto characterAtOffset = toAXElement(thisObject)->characterAtOffset(offset);
    return JSValueMakeString(context, characterAtOffset.get());
}

static JSValueRef wordAtOffsetCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int offset = -1;
    if (argumentCount == 1)
        offset = JSValueToNumber(context, arguments[0], exception);

    auto wordAtOffset = toAXElement(thisObject)->wordAtOffset(offset);
    return JSValueMakeString(context, wordAtOffset.get());
}

static JSValueRef lineAtOffsetCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int offset = -1;
    if (argumentCount == 1)
        offset = JSValueToNumber(context, arguments[0], exception);

    auto lineAtOffset = toAXElement(thisObject)->lineAtOffset(offset);
    return JSValueMakeString(context, lineAtOffset.get());
}

static JSValueRef sentenceAtOffsetCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int offset = -1;
    if (argumentCount == 1)
        offset = JSValueToNumber(context, arguments[0], exception);

    auto sentenceAtOffset = toAXElement(thisObject)->sentenceAtOffset(offset);
    return JSValueMakeString(context, sentenceAtOffset.get());
}

#elif PLATFORM(IOS_FAMILY)

static JSValueRef getIsSearchFieldCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isSearchField());
}

static JSValueRef getIsTextAreaCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->isTextArea());
}

static JSValueRef stringForSelectionCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto labelString = toAXElement(thisObject)->stringForSelection();
    return JSValueMakeString(context, labelString.get());
}

static JSValueRef getIdentifierCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto valueString = toAXElement(thisObject)->identifier();
    return JSValueMakeString(context, valueString.get());
}


static JSValueRef getTraitsCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto valueString = toAXElement(thisObject)->traits();
    return JSValueMakeString(context, valueString.get());
}

static JSValueRef getElementTextPositionCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->elementTextPosition());
}

static JSValueRef getElementTextLengthCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->elementTextLength());
}

static JSValueRef hasContainedByFieldsetTraitCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef, JSValueRef*)
{
    return JSValueMakeBoolean(context, toAXElement(thisObject)->hasContainedByFieldsetTrait());
}

static JSValueRef textMarkerRangeMatchesTextNearMarkersCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSStringRef searchText = nullptr;
    AccessibilityTextMarker* startMarker = nullptr;
    AccessibilityTextMarker* endMarker = nullptr;
    if (argumentCount == 3) {
        searchText = JSValueToStringCopy(context, arguments[0], exception);
        startMarker = toTextMarker(JSValueToObject(context, arguments[1], exception));
        endMarker = toTextMarker(JSValueToObject(context, arguments[2], exception));
    }
    
    JSValueRef result = AccessibilityTextMarkerRange::makeJSAccessibilityTextMarkerRange(context, toAXElement(thisObject)->textMarkerRangeMatchesTextNearMarkers(searchText, startMarker, endMarker));
    if (searchText)
        JSStringRelease(searchText);
    return result;
}

#endif // PLATFORM(IOS_FAMILY)

#if PLATFORM(MAC) && !PLATFORM(IOS_FAMILY)
static JSValueRef supportedActionsCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto valueString = toAXElement(thisObject)->supportedActions();
    return JSValueMakeString(context, valueString.get());
}

static JSValueRef mathPostscriptsDescriptionCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto valueString = toAXElement(thisObject)->mathPostscriptsDescription();
    return JSValueMakeString(context, valueString.get());
}

static JSValueRef mathPrescriptsDescriptionCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    auto valueString = toAXElement(thisObject)->mathPrescriptsDescription();
    return JSValueMakeString(context, valueString.get());
}

#endif

// Implementation

// Unsupported methods on various platforms.
#if !PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForLine(int line) { return 0; }
JSRetainPtr<JSStringRef> AccessibilityUIElement::rangeForPosition(int, int) { return 0; }
void AccessibilityUIElement::setSelectedChild(AccessibilityUIElement*) const { }
void AccessibilityUIElement::setSelectedChildAtIndex(unsigned) const { }
void AccessibilityUIElement::removeSelectionAtIndex(unsigned) const { }
AccessibilityUIElement AccessibilityUIElement::horizontalScrollbar() const { return { nullptr }; }
AccessibilityUIElement AccessibilityUIElement::verticalScrollbar() const { return { nullptr }; }
AccessibilityUIElement AccessibilityUIElement::uiElementAttributeValue(JSStringRef) const { return { nullptr }; }
#endif

#if !PLATFORM(MAC) && !PLATFORM(IOS_FAMILY)
JSRetainPtr<JSStringRef> AccessibilityUIElement::speakAs() { return nullptr; }
JSRetainPtr<JSStringRef> AccessibilityUIElement::pathDescription() const { return 0; }
void AccessibilityUIElement::setValue(JSStringRef) { }
#endif

#if !PLATFORM(COCOA)
void AccessibilityUIElement::uiElementArrayAttributeValue(JSStringRef, Vector<AccessibilityUIElement>&) const { }
#endif

#if !PLATFORM(WIN)
bool AccessibilityUIElement::isEqual(AccessibilityUIElement* otherElement)
{
    if (!otherElement)
        return false;
    return platformUIElement() == otherElement->platformUIElement();
}
#endif

#if !PLATFORM(MAC)
void AccessibilityUIElement::setBoolAttributeValue(JSStringRef, bool) { }
#endif

#if !SUPPORTS_AX_TEXTMARKERS

AccessibilityTextMarkerRange AccessibilityUIElement::lineTextMarkerRangeForTextMarker(AccessibilityTextMarker*)
{
    return nullptr;
}

AccessibilityTextMarkerRange AccessibilityUIElement::textMarkerRangeForElement(AccessibilityUIElement*)
{
    return 0;
}

AccessibilityTextMarkerRange AccessibilityUIElement::selectedTextMarkerRange()
{
    return nullptr;
}

void AccessibilityUIElement::resetSelectedTextMarkerRange()
{
}

bool AccessibilityUIElement::replaceTextInRange(JSStringRef, int, int)
{
    return false;
}

bool AccessibilityUIElement::insertText(JSStringRef)
{
    return false;
}

int AccessibilityUIElement::textMarkerRangeLength(AccessibilityTextMarkerRange*)
{
    return 0;
}

AccessibilityTextMarkerRange AccessibilityUIElement::textMarkerRangeForMarkers(AccessibilityTextMarker*, AccessibilityTextMarker*)
{
    return 0;
}

AccessibilityTextMarker AccessibilityUIElement::startTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    return 0;
}

AccessibilityTextMarker AccessibilityUIElement::endTextMarkerForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    return 0;   
}

AccessibilityUIElement AccessibilityUIElement::accessibilityElementForTextMarker(AccessibilityTextMarker*)
{
    return { nullptr };
}

AccessibilityTextMarker AccessibilityUIElement::endTextMarkerForBounds(int x, int y, int width, int height)
{
    return 0;
}

AccessibilityTextMarker AccessibilityUIElement::startTextMarkerForBounds(int x, int y, int width, int height)
{
    return 0;
}

AccessibilityTextMarker AccessibilityUIElement::textMarkerForPoint(int x, int y)
{
    return 0;
}

AccessibilityTextMarker AccessibilityUIElement::previousTextMarker(AccessibilityTextMarker*)
{
    return 0;    
}

AccessibilityTextMarker AccessibilityUIElement::nextTextMarker(AccessibilityTextMarker*)
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRange(AccessibilityTextMarkerRange*)
{
    return nullptr;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForTextMarkerRangeWithOptions(AccessibilityTextMarkerRange*, bool includeSpellCheck)
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

bool AccessibilityUIElement::isTextMarkerValid(AccessibilityTextMarker*)
{
    return false;
}

AccessibilityTextMarker AccessibilityUIElement::textMarkerForIndex(int)
{
    return 0;
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

#if PLATFORM(IOS_FAMILY)
AccessibilityTextMarkerRange AccessibilityUIElement::textMarkerRangeMatchesTextNearMarkers(JSStringRef, AccessibilityTextMarker*, AccessibilityTextMarker*)
{
    return nullptr;
}
#endif

#endif

// Destruction

static void finalize(JSObjectRef thisObject)
{
    delete toAXElement(thisObject);
}

// Object Creation

JSObjectRef AccessibilityUIElement::makeJSAccessibilityUIElement(JSContextRef context, const AccessibilityUIElement& element)
{
    if (!element.platformUIElement())
        return nullptr;

    return JSObjectMake(context, AccessibilityUIElement::getJSClass(), new AccessibilityUIElement(element));
}

JSClassRef AccessibilityUIElement::getJSClass()
{
    static JSStaticValue staticValues[] = {
        { "accessibilityValue", getAccessibilityValueCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "role", getRoleCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "subrole", getSubroleCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "roleDescription", getRoleDescriptionCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "computedRoleString", getComputedRoleStringCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "title", getTitleCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "description", getDescriptionCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "language", getLanguageCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "helpText", getHelpTextCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "stringValue", getStringValueCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "x", getXCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "y", getYCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "width", getWidthCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "height", getHeightCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "clickPointX", getClickPointXCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "clickPointY", getClickPointYCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "intValue", getIntValueCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "minValue", getMinValueCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "maxValue", getMaxValueCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pathDescription", getPathDescriptionCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "childrenCount", getChildrenCountCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "rowCount", rowCountCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "columnCount", columnCountCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "insertionPointLineNumber", getInsertionPointLineNumberCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "selectedTextRange", getSelectedTextRangeCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isEnabled", getIsEnabledCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isRequired", getIsRequiredCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isFocused", getIsFocusedCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isFocusable", getIsFocusableCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isSelected", getIsSelectedCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isSelectable", getIsSelectableCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isMultiSelectable", getIsMultiSelectableCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isSelectedOptionActive", getIsSelectedOptionActiveCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isExpanded", getIsExpandedCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isChecked", getIsCheckedCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isIndeterminate", getIsIndeterminate, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isVisible", getIsVisibleCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isOffScreen", getIsOffScreenCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isCollapsed", getIsCollapsedCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "hasPopup", getHasPopupCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "popupValue", getPopupValueCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "valueDescription", getValueDescriptionCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "hierarchicalLevel", hierarchicalLevelCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "documentEncoding", getDocumentEncodingCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "documentURI", getDocumentURICallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "url", getURLCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isValid", getIsValidCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "orientation", getOrientationCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "ariaIsGrabbed", getARIAIsGrabbedCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "ariaDropEffects", getARIADropEffectsCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "classList", getClassListCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isIgnored", isIgnoredCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "speakAs", speakAsCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "selectedChildrenCount", selectedChildrenCountCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "horizontalScrollbar", horizontalScrollbarCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "verticalScrollbar", verticalScrollbarCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "startTextMarker", startTextMarkerCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "endTextMarker", endTextMarkerCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
#if PLATFORM(IOS_FAMILY)
        { "identifier", getIdentifierCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "traits", getTraitsCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "elementTextPosition", getElementTextPositionCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "elementTextLength", getElementTextLengthCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "stringForSelection", stringForSelectionCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "hasContainedByFieldsetTrait", hasContainedByFieldsetTraitCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isSearchField", getIsSearchFieldCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isTextArea", getIsTextAreaCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
#endif // PLATFORM(IOS_FAMILY)
#if PLATFORM(MAC) && !PLATFORM(IOS_FAMILY)
        { "supportedActions", supportedActionsCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "mathPostscriptsDescription", mathPostscriptsDescriptionCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "mathPrescriptsDescription", mathPrescriptsDescriptionCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
#endif
        { 0, 0, 0, 0 }
    };

    static JSStaticFunction staticFunctions[] = {
        { "allAttributes", allAttributesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfLinkedUIElements", attributesOfLinkedUIElementsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfDocumentLinks", attributesOfDocumentLinksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfChildren", attributesOfChildrenCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "parameterizedAttributeNames", parameterizedAttributeNamesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "lineForIndex", lineForIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "rangeForLine", rangeForLineCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "boundsForRange", boundsForRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "rangeForPosition", rangeForPositionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "stringForRange", stringForRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributedStringForRange", attributedStringForRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributedStringRangeIsMisspelled", attributedStringRangeIsMisspelledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "uiElementCountForSearchPredicate", uiElementCountForSearchPredicateCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "uiElementForSearchPredicate", uiElementForSearchPredicateCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "selectTextWithCriteria", selectTextWithCriteriaCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
#if PLATFORM(MAC)
        { "searchTextWithCriteria", searchTextWithCriteriaCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
#endif
        { "childAtIndex", childAtIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "linkedUIElementAtIndex", linkedUIElementAtIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "indexOfChild", indexOfChildCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "elementAtPoint", elementAtPointCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfColumnHeaders", attributesOfColumnHeadersCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfRowHeaders", attributesOfRowHeadersCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfColumns", attributesOfColumnsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfRows", attributesOfRowsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfVisibleCells", attributesOfVisibleCellsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfHeader", attributesOfHeaderCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "indexInTable", indexInTableCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "rowIndexRange", rowIndexRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "columnIndexRange", columnIndexRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "cellForColumnAndRow", cellForColumnAndRowCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "titleUIElement", titleUIElementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setSelectedTextRange", setSelectedTextRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "stringAttributeValue", stringAttributeValueCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "uiElementArrayAttributeValue", uiElementArrayAttributeValueCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "rowHeaders", rowHeadersCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "columnHeaders", columnHeadersCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "uiElementAttributeValue", uiElementAttributeValueCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "numberAttributeValue", numberAttributeValueCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "boolAttributeValue", boolAttributeValueCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setBoolAttributeValue", setBoolAttributeValueCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isAttributeSupported", isAttributeSupportedCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isAttributeSettable", isAttributeSettableCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isPressActionSupported", isPressActionSupportedCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isIncrementActionSupported", isIncrementActionSupportedCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isDecrementActionSupported", isDecrementActionSupportedCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "parentElement", parentElementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "disclosedByRow", disclosedByRowCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "increment", incrementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "decrement", decrementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "showMenu", showMenuCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "press", pressCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "disclosedRowAtIndex", disclosedRowAtIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "ariaOwnsElementAtIndex", ariaOwnsElementAtIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "ariaFlowToElementAtIndex", ariaFlowToElementAtIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "ariaControlsElementAtIndex", ariaControlsElementAtIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "selectedRowAtIndex", selectedRowAtIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "rowAtIndex", rowAtIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isEqual", isEqualCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "addNotificationListener", addNotificationListenerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "removeNotificationListener", removeNotificationListenerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "takeFocus", takeFocusCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "takeSelection", takeSelectionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "addSelection", addSelectionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "removeSelection", removeSelectionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "lineTextMarkerRangeForTextMarker", lineTextMarkerRangeForTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "textMarkerRangeForElement", textMarkerRangeForElementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "selectedTextMarkerRange", selectedTextMarkerRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "resetSelectedTextMarkerRange", resetSelectedTextMarkerRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "replaceTextInRange", replaceTextInRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "insertText", insertTextCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributedStringForTextMarkerRangeContainsAttribute", attributedStringForTextMarkerRangeContainsAttributeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "indexForTextMarker", indexForTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isTextMarkerValid", isTextMarkerValidCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "textMarkerRangeForMarkers", textMarkerRangeForMarkersCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "textMarkerForIndex", textMarkerForIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "startTextMarkerForTextMarkerRange", startTextMarkerForTextMarkerRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "endTextMarkerForTextMarkerRange", endTextMarkerForTextMarkerRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "accessibilityElementForTextMarker", accessibilityElementForTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "textMarkerRangeLength", textMarkerRangeLengthCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "endTextMarkerForBounds", endTextMarkerForBoundsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "startTextMarkerForBounds", startTextMarkerForBoundsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "textMarkerForPoint", textMarkerForPointCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "nextTextMarker", nextTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "previousTextMarker", previousTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "stringForTextMarkerRange", stringForTextMarkerRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributedStringForTextMarkerRange", attributedStringForTextMarkerRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributedStringForTextMarkerRangeWithOptions", attributedStringForTextMarkerRangeWithOptionsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "leftWordTextMarkerRangeForTextMarker", leftWordTextMarkerRangeForTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "rightWordTextMarkerRangeForTextMarker", rightWordTextMarkerRangeForTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "previousWordStartTextMarkerForTextMarker", previousWordStartTextMarkerForTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "nextWordEndTextMarkerForTextMarker", nextWordEndTextMarkerForTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "paragraphTextMarkerRangeForTextMarker", paragraphTextMarkerRangeForTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "previousParagraphStartTextMarkerForTextMarker", previousParagraphStartTextMarkerForTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "nextParagraphEndTextMarkerForTextMarker", nextParagraphEndTextMarkerForTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "sentenceTextMarkerRangeForTextMarker", sentenceTextMarkerRangeForTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "previousSentenceStartTextMarkerForTextMarker", previousSentenceStartTextMarkerForTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "nextSentenceEndTextMarkerForTextMarker", nextSentenceEndTextMarkerForTextMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setSelectedChild", setSelectedChildCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setSelectedChildAtIndex", setSelectedChildAtIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "removeSelectionAtIndex", removeSelectionAtIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setValue", setValueCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setSelectedVisibleTextRange", setSelectedVisibleTextRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "selectedChildAtIndex", selectedChildAtIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "scrollToMakeVisible", scrollToMakeVisibleCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "scrollToGlobalPoint", scrollToGlobalPointCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "scrollToMakeVisibleWithSubFocus", scrollToMakeVisibleWithSubFocusCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
#if PLATFORM(GTK)
        { "characterAtOffset", characterAtOffsetCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "wordAtOffset", wordAtOffsetCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "lineAtOffset", lineAtOffsetCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "sentenceAtOffset", sentenceAtOffsetCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
#elif PLATFORM(IOS_FAMILY)
        { "linkedElement", linkedElementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "headerElementAtIndex", headerElementAtIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "elementsForRange", elementsForRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "increaseTextSelection", increaseTextSelectionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "decreaseTextSelection", decreaseTextSelectionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "scrollPageUp", scrollPageUpCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "scrollPageDown", scrollPageDownCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "scrollPageLeft", scrollPageLeftCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "scrollPageRight", scrollPageRightCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "assistiveTechnologySimulatedFocus", assistiveTechnologySimulatedFocusCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "fieldsetAncestorElement", fieldsetAncestorElementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "textMarkerRangeMatchesTextNearMarkers", textMarkerRangeMatchesTextNearMarkersCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributedStringForElement", attributedStringForElementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
#endif
        { 0, 0, 0 }
    };

    static JSClassDefinition classDefinition = {
        0, kJSClassAttributeNone, "AccessibilityUIElement", 0, staticValues, staticFunctions,
        0, finalize, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    static JSClassRef accessibilityUIElementClass = JSClassCreate(&classDefinition);
    return accessibilityUIElementClass;
}
#endif
