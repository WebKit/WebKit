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
#include "AccessibilityUIElement.h"

#include <JavaScriptCore/JSRetainPtr.h>

// Static Functions

static inline AccessibilityUIElement* toAXElement(JSObjectRef object)
{
    // FIXME: We should ASSERT that it is the right class here.
    return static_cast<AccessibilityUIElement*>(JSObjectGetPrivate(object));
}

static JSValueRef allAttributesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> attributes(Adopt, toAXElement(thisObject)->allAttributes());
    return JSValueMakeString(context, attributes.get());
}

static JSValueRef attributesOfLinkedUIElementsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> linkedUIDescription(Adopt, toAXElement(thisObject)->attributesOfLinkedUIElements());
    return JSValueMakeString(context, linkedUIDescription.get());
}

static JSValueRef attributesOfDocumentLinksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> linkedUIDescription(Adopt, toAXElement(thisObject)->attributesOfDocumentLinks());
    return JSValueMakeString(context, linkedUIDescription.get());
}

static JSValueRef attributesOfChildrenCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> childrenDescription(Adopt, toAXElement(thisObject)->attributesOfChildren());
    return JSValueMakeString(context, childrenDescription.get());
}

static JSValueRef parameterizedAttributeNamesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> parameterizedAttributeNames(Adopt, toAXElement(thisObject)->parameterizedAttributeNames());
    return JSValueMakeString(context, parameterizedAttributeNames.get());
}

static JSValueRef attributesOfColumnHeadersCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> attributesOfColumnHeaders(Adopt, toAXElement(thisObject)->attributesOfColumnHeaders());
    return JSValueMakeString(context, attributesOfColumnHeaders.get());
}

static JSValueRef attributesOfRowHeadersCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> attributesOfRowHeaders(Adopt, toAXElement(thisObject)->attributesOfRowHeaders());
    return JSValueMakeString(context, attributesOfRowHeaders.get());
}

static JSValueRef attributesOfColumnsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> attributesOfColumns(Adopt, toAXElement(thisObject)->attributesOfColumns());
    return JSValueMakeString(context, attributesOfColumns.get());
}

static JSValueRef attributesOfRowsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> attributesOfRows(Adopt, toAXElement(thisObject)->attributesOfRows());
    return JSValueMakeString(context, attributesOfRows.get());
}

static JSValueRef attributesOfVisibleCellsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> attributesOfVisibleCells(Adopt, toAXElement(thisObject)->attributesOfVisibleCells());
    return JSValueMakeString(context, attributesOfVisibleCells.get());
}

static JSValueRef attributesOfHeaderCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> attributesOfHeader(Adopt, toAXElement(thisObject)->attributesOfHeader());
    return JSValueMakeString(context, attributesOfHeader.get());
}

static JSValueRef indexInTableCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->indexInTable());
}

static JSValueRef rowIndexRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> rowIndexRange(Adopt, toAXElement(thisObject)->rowIndexRange());
    return JSValueMakeString(context, rowIndexRange.get());
}

static JSValueRef columnIndexRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> columnIndexRange(Adopt, toAXElement(thisObject)->columnIndexRange());
    return JSValueMakeString(context, columnIndexRange.get());
}

static JSValueRef lineForIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = -1;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);
    
    return JSValueMakeNumber(context, toAXElement(thisObject)->lineForIndex(indexNumber));
}

static JSValueRef boundsForRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    unsigned location = UINT_MAX, length = 0;
    if (argumentCount == 2) {
        location = JSValueToNumber(context, arguments[0], exception);
        length = JSValueToNumber(context, arguments[1], exception);
    }

    JSRetainPtr<JSStringRef> boundsDescription(Adopt, toAXElement(thisObject)->boundsForRange(location, length));
    return JSValueMakeString(context, boundsDescription.get());    
}

static JSValueRef stringForRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    unsigned location = UINT_MAX, length = 0;
    if (argumentCount == 2) {
        location = JSValueToNumber(context, arguments[0], exception);
        length = JSValueToNumber(context, arguments[1], exception);
    }
    
    JSRetainPtr<JSStringRef> stringDescription(Adopt, toAXElement(thisObject)->stringForRange(location, length));
    return JSValueMakeString(context, stringDescription.get());    
}

static JSValueRef childAtIndexCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int indexNumber = -1;
    if (argumentCount == 1)
        indexNumber = JSValueToNumber(context, arguments[0], exception);
    
    return AccessibilityUIElement::makeJSAccessibilityUIElement(context, toAXElement(thisObject)->getChildAtIndex(indexNumber));
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


static JSValueRef isAttributeSettableCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSStringRef attribute = NULL;
    if (argumentCount == 1)
        attribute = JSValueToStringCopy(context, arguments[0], exception);    
    JSValueRef result = JSValueMakeNumber(context, toAXElement(thisObject)->isAttributeSettable(attribute));
    if (attribute)
        JSStringRelease(attribute);
    return result;
}


static JSValueRef isActionSupportedCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSStringRef action = 0;
    if (argumentCount == 1)
        action = JSValueToStringCopy(context, arguments[0], exception);    
    JSValueRef result = JSValueMakeNumber(context, toAXElement(thisObject)->isActionSupported(action));
    if (action)
        JSStringRelease(action);
    return result;
}

static JSValueRef attributeValueCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    JSStringRef attribute = NULL;
    if (argumentCount == 1)
        attribute = JSValueToStringCopy(context, arguments[0], exception);
    JSRetainPtr<JSStringRef> attributeValue(Adopt, toAXElement(thisObject)->attributeValue(attribute));
    JSValueRef result = JSValueMakeString(context, attributeValue.get());
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

static JSValueRef setSelectedTextRangeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    unsigned location = UINT_MAX, length = 0;
    if (argumentCount == 2) {
        location = JSValueToNumber(context, arguments[0], exception);
        length = JSValueToNumber(context, arguments[1], exception);
    }
    
    toAXElement(thisObject)->setSelectedTextRange(location, length);
    return 0;
}

static JSValueRef incrementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->increment();
    return 0;
}

static JSValueRef decrementCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    toAXElement(thisObject)->decrement();
    return 0;
}


// Static Value Getters

static JSValueRef getRoleCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> role(Adopt, toAXElement(thisObject)->role());
    return JSValueMakeString(context, role.get());
}

static JSValueRef getSubroleCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> role(Adopt, toAXElement(thisObject)->subrole());
    return JSValueMakeString(context, role.get());
}

static JSValueRef getTitleCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> title(Adopt, toAXElement(thisObject)->title());
    return JSValueMakeString(context, title.get());
}

static JSValueRef getDescriptionCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> description(Adopt, toAXElement(thisObject)->description());
    return JSValueMakeString(context, description.get());
}

static JSValueRef getLanguageCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> language(Adopt, toAXElement(thisObject)->language());
    return JSValueMakeString(context, language.get());
}

static JSValueRef getChildrenCountCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    return JSValueMakeNumber(context, toAXElement(thisObject)->childrenCount());
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

static JSValueRef getSelectedTextRangeCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> selectedTextRange(Adopt, toAXElement(thisObject)->selectedTextRange());
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

static JSValueRef getValueDescriptionCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> valueDescription(Adopt, toAXElement(thisObject)->valueDescription());
    return JSValueMakeString(context, valueDescription.get());
}

static JSValueRef getAccessibilityValueCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> accessibilityValue(Adopt, toAXElement(thisObject)->accessibilityValue());
    return JSValueMakeString(context, accessibilityValue.get());
}

// Destruction

static void finalize(JSObjectRef thisObject)
{
    delete toAXElement(thisObject);
}

// Object Creation

JSObjectRef AccessibilityUIElement::makeJSAccessibilityUIElement(JSContextRef context, const AccessibilityUIElement& element)
{
    return JSObjectMake(context, AccessibilityUIElement::getJSClass(), new AccessibilityUIElement(element));
}

JSClassRef AccessibilityUIElement::getJSClass()
{
    static JSStaticValue staticValues[] = {
        { "accessibilityValue", getAccessibilityValueCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "role", getRoleCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "subrole", getSubroleCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "title", getTitleCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "description", getDescriptionCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "language", getLanguageCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "x", getXCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "y", getYCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "width", getWidthCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "height", getHeightCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "clickPointX", getClickPointXCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "clickPointY", getClickPointYCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "intValue", getIntValueCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "minValue", getMinValueCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "maxValue", getMaxValueCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "childrenCount", getChildrenCountCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "insertionPointLineNumber", getInsertionPointLineNumberCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "selectedTextRange", getSelectedTextRangeCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isEnabled", getIsEnabledCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isRequired", getIsRequiredCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "valueDescription", getValueDescriptionCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0, 0 }
    };

    static JSStaticFunction staticFunctions[] = {
        { "allAttributes", allAttributesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfLinkedUIElements", attributesOfLinkedUIElementsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfDocumentLinks", attributesOfDocumentLinksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "attributesOfChildren", attributesOfChildrenCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "parameterizedAttributeNames", parameterizedAttributeNamesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "lineForIndex", lineForIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "boundsForRange", boundsForRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "stringForRange", stringForRangeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "childAtIndex", childAtIndexCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
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
        { "attributeValue", attributeValueCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isAttributeSettable", isAttributeSettableCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isActionSupported", isActionSupportedCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "parentElement", parentElementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "increment", incrementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "decrement", decrementCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 }
    };

    static JSClassDefinition classDefinition = {
        0, kJSClassAttributeNone, "AccessibilityUIElement", 0, staticValues, staticFunctions,
        0, finalize, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    static JSClassRef accessibilityUIElementClass = JSClassCreate(&classDefinition);
    return accessibilityUIElementClass;
}
