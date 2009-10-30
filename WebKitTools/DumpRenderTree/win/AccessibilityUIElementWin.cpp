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

#include "config.h"
#include "AccessibilityUIElement.h"

#include <JavaScriptCore/JSStringRef.h>
#include <tchar.h>
#include <string>

using std::wstring;

AccessibilityUIElement::AccessibilityUIElement(PlatformUIElement element)
    : m_element(element)
{
}

AccessibilityUIElement::AccessibilityUIElement(const AccessibilityUIElement& other)
    : m_element(other.m_element)
{
}

AccessibilityUIElement::~AccessibilityUIElement()
{
}

void AccessibilityUIElement::getLinkedUIElements(Vector<AccessibilityUIElement>&)
{
}

void AccessibilityUIElement::getDocumentLinks(Vector<AccessibilityUIElement>&)
{
}

void AccessibilityUIElement::getChildren(Vector<AccessibilityUIElement>& children)
{
    long childCount;
    if (FAILED(m_element->get_accChildCount(&childCount)))
        return;
    for (long i = 0; i < childCount; ++i)
        children.append(getChildAtIndex(i));
}

void AccessibilityUIElement::getChildrenWithRange(Vector<AccessibilityUIElement>& elementVector, unsigned location, unsigned length)
{
    long childCount;
    unsigned appendedCount = 0;
    if (FAILED(m_element->get_accChildCount(&childCount)))
        return;
    for (long i = location; i < childCount && appendedCount < length; ++i, ++appendedCount)
        elementVector.append(getChildAtIndex(i));
}

int AccessibilityUIElement::childrenCount()
{
    long childCount;
    m_element->get_accChildCount(&childCount);
    return childCount;
}

AccessibilityUIElement AccessibilityUIElement::elementAtPoint(int x, int y)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::getChildAtIndex(unsigned index)
{
    COMPtr<IDispatch> child;
    VARIANT vChild;
    ::VariantInit(&vChild);
    V_VT(&vChild) = VT_I4;
    // In MSAA, index 0 is the object itself.
    V_I4(&vChild) = index + 1;
    if (FAILED(m_element->get_accChild(vChild, &child)))
        return 0;
    return COMPtr<IAccessible>(Query, child);
}

JSStringRef AccessibilityUIElement::allAttributes()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfLinkedUIElements()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::attributesOfDocumentLinks()
{
    return JSStringCreateWithCharacters(0, 0);
}

AccessibilityUIElement AccessibilityUIElement::titleUIElement()
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::parentElement()
{
    return 0;
}

JSStringRef AccessibilityUIElement::attributesOfChildren()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::parameterizedAttributeNames()
{
    return JSStringCreateWithCharacters(0, 0);
}

static VARIANT& self()
{
    static VARIANT vSelf;
    static bool haveInitialized;

    if (!haveInitialized) {
        ::VariantInit(&vSelf);
        V_VT(&vSelf) = VT_I4;
        V_I4(&vSelf) = CHILDID_SELF;
    }
    return vSelf;
}

JSStringRef AccessibilityUIElement::role()
{
    VARIANT vRole;
    if (FAILED(m_element->get_accRole(self(), &vRole)))
        return JSStringCreateWithCharacters(0, 0);
    ASSERT(V_VT(&vRole) == VT_I4);
    TCHAR roleText[64] = {0};
    ::GetRoleText(V_I4(&vRole), roleText, ARRAYSIZE(roleText));
    return JSStringCreateWithCharacters(roleText, _tcslen(roleText));
}

JSStringRef AccessibilityUIElement::subrole()
{
    return 0;
}

JSStringRef AccessibilityUIElement::title()
{
    BSTR titleBSTR;
    if (FAILED(m_element->get_accName(self(), &titleBSTR)) || !titleBSTR)
        return JSStringCreateWithCharacters(0, 0);
    wstring title(titleBSTR, SysStringLen(titleBSTR));
    ::SysFreeString(titleBSTR);
    return JSStringCreateWithCharacters(title.data(), title.length());
}

JSStringRef AccessibilityUIElement::description()
{
    BSTR descriptionBSTR;
    if (FAILED(m_element->get_accName(self(), &descriptionBSTR)) || !descriptionBSTR)
        return JSStringCreateWithCharacters(0, 0);
    wstring description(descriptionBSTR, SysStringLen(descriptionBSTR));
    ::SysFreeString(descriptionBSTR);
    return JSStringCreateWithCharacters(description.data(), description.length());
}

JSStringRef AccessibilityUIElement::language()
{
    return JSStringCreateWithCharacters(0, 0);
}

double AccessibilityUIElement::x()
{
    long x, y, width, height;
    if (FAILED(m_element->accLocation(&x, &y, &width, &height, self())))
        return 0;
    return x;
}

double AccessibilityUIElement::y()
{
    long x, y, width, height;
    if (FAILED(m_element->accLocation(&x, &y, &width, &height, self())))
        return 0;
    return y;
}

double AccessibilityUIElement::width()
{
    long x, y, width, height;
    if (FAILED(m_element->accLocation(&x, &y, &width, &height, self())))
        return 0;
    return width;
}

double AccessibilityUIElement::height()
{
    long x, y, width, height;
    if (FAILED(m_element->accLocation(&x, &y, &width, &height, self())))
        return 0;
    return height;
}

double AccessibilityUIElement::clickPointX()
{
    return 0;
}

double AccessibilityUIElement::clickPointY()
{
    return 0;
}

JSStringRef AccessibilityUIElement::valueDescription()
{
    return 0;
}

double AccessibilityUIElement::intValue()
{
    BSTR valueBSTR;
    if (FAILED(m_element->get_accValue(self(), &valueBSTR)) || !valueBSTR)
        return 0;
    wstring value(valueBSTR, SysStringLen(valueBSTR));
    ::SysFreeString(valueBSTR);
    TCHAR* ignored;
    return _tcstod(value.data(), &ignored);
}

double AccessibilityUIElement::minValue()
{
    return 0;
}

double AccessibilityUIElement::maxValue()
{
    return 0;
}

bool AccessibilityUIElement::isActionSupported(JSStringRef action)
{
    return false;
}

bool AccessibilityUIElement::isEnabled()
{
    return false;
}

bool AccessibilityUIElement::isRequired() const
{
    return false;
}

int AccessibilityUIElement::insertionPointLineNumber()
{
    return 0;
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

int AccessibilityUIElement::indexInTable()
{
    return 0;
}

JSStringRef AccessibilityUIElement::rowIndexRange()
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::columnIndexRange()
{
    return JSStringCreateWithCharacters(0, 0);
}

int AccessibilityUIElement::lineForIndex(int)
{
    return 0;
}

JSStringRef AccessibilityUIElement::boundsForRange(unsigned location, unsigned length)
{
    return JSStringCreateWithCharacters(0, 0);
}

JSStringRef AccessibilityUIElement::stringForRange(unsigned, unsigned)
{
    return JSStringCreateWithCharacters(0, 0);
}

AccessibilityUIElement AccessibilityUIElement::cellForColumnAndRow(unsigned column, unsigned row)
{
    return 0;
}

JSStringRef AccessibilityUIElement::selectedTextRange()
{
    return JSStringCreateWithCharacters(0, 0);    
}

void AccessibilityUIElement::setSelectedTextRange(unsigned location, unsigned length)
{
}

JSStringRef AccessibilityUIElement::attributeValue(JSStringRef attribute)
{
    return JSStringCreateWithCharacters(0, 0);
}

bool AccessibilityUIElement::isAttributeSettable(JSStringRef attribute)
{
    return false;
}

void AccessibilityUIElement::increment()
{
}

void AccessibilityUIElement::decrement()
{
}

JSStringRef AccessibilityUIElement::accessibilityValue() const
{
    BSTR valueBSTR;
    if (FAILED(m_element->get_accValue(self(), &valueBSTR)) || !valueBSTR)
        return JSStringCreateWithCharacters(0, 0);

    wstring value(valueBSTR, SysStringLen(valueBSTR));
    ::SysFreeString(valueBSTR);

    return JSStringCreateWithCharacters(value.data(), value.length());
}
