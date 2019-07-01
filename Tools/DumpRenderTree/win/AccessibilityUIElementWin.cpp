/*
 * Copyright (C) 2008-2018 Apple Inc. All Rights Reserved.
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

#include "AccessibilityController.h"
#include "DumpRenderTree.h"
#include "FrameLoadDelegate.h"
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSStringRefBSTR.h>
#include <wtf/text/WTFString.h>
#include <comutil.h>
#include <tchar.h>
#include <string>

using std::wstring;

static JSRetainPtr<JSStringRef> createEmptyJSString()
{
    return adopt(JSStringCreateWithCharacters(nullptr, 0));
}

static COMPtr<IAccessibleComparable> comparableObject(IAccessible* accessible)
{
    COMPtr<IServiceProvider> serviceProvider(Query, accessible);
    if (!serviceProvider)
        return 0;
    COMPtr<IAccessibleComparable> comparable;
    serviceProvider->QueryService(SID_AccessibleComparable, __uuidof(IAccessibleComparable), reinterpret_cast<void**>(&comparable));
    return comparable;
}

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

bool AccessibilityUIElement::isEqual(AccessibilityUIElement* otherElement)
{
    COMPtr<IAccessibleComparable> comparable = comparableObject(m_element.get());
    COMPtr<IAccessibleComparable> otherComparable = comparableObject(otherElement->m_element.get());
    if (!comparable || !otherComparable)
        return false;
    BOOL isSame = FALSE;
    if (FAILED(comparable->isSameObject(otherComparable.get(), &isSame)))
        return false;
    return isSame;
}

void AccessibilityUIElement::getLinkedUIElements(Vector<AccessibilityUIElement>&)
{
}

void AccessibilityUIElement::getDocumentLinks(Vector<AccessibilityUIElement>&)
{
}

void AccessibilityUIElement::getChildren(Vector<AccessibilityUIElement>& children)
{
    if (!m_element)
        return;

    long childCount;
    if (FAILED(m_element->get_accChildCount(&childCount)))
        return;
    for (long i = 0; i < childCount; ++i)
        children.append(getChildAtIndex(i));
}

void AccessibilityUIElement::getChildrenWithRange(Vector<AccessibilityUIElement>& elementVector, unsigned location, unsigned length)
{
    if (!m_element)
        return;

    long childCount;
    unsigned appendedCount = 0;
    if (FAILED(m_element->get_accChildCount(&childCount)))
        return;
    for (long i = location; i < childCount && appendedCount < length; ++i, ++appendedCount)
        elementVector.append(getChildAtIndex(i));
}

int AccessibilityUIElement::childrenCount()
{
    if (!m_element)
        return 0;

    long childCount;
    if (FAILED(m_element->get_accChildCount(&childCount)))
        return 0;

    return childCount;
}

int AccessibilityUIElement::rowCount()
{
    // FIXME: implement
    return 0;
}
 
int AccessibilityUIElement::columnCount()
{
    // FIXME: implement
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::elementAtPoint(int x, int y)
{
    return { nullptr };
}

AccessibilityUIElement AccessibilityUIElement::linkedUIElementAtIndex(unsigned index)
{
    // FIXME: implement
    return { nullptr };
}

AccessibilityUIElement AccessibilityUIElement::getChildAtIndex(unsigned index)
{
    if (!m_element)
        return { nullptr };

    COMPtr<IDispatch> child;
    _variant_t vChild;
    V_VT(&vChild) = VT_I4;
    // In MSAA, index 0 is the object itself.
    V_I4(&vChild) = index + 1;
    if (FAILED(m_element->get_accChild(vChild.GetVARIANT(), &child)))
        return { nullptr };
    return COMPtr<IAccessible>(Query, child);
}

unsigned AccessibilityUIElement::indexOfChild(AccessibilityUIElement* element)
{ 
    // FIXME: implement
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::allAttributes()
{
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfLinkedUIElements()
{
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfDocumentLinks()
{
    return createEmptyJSString();
}

AccessibilityUIElement AccessibilityUIElement::titleUIElement()
{
    COMPtr<IAccessible> platformElement = platformUIElement();

    COMPtr<IAccessibleComparable> comparable = comparableObject(platformElement.get());
    if (!comparable)
        return { nullptr };

    _variant_t value;
    _bstr_t titleUIElementAttributeKey(L"AXTitleUIElementAttribute");
    if (FAILED(comparable->get_attribute(titleUIElementAttributeKey, &value.GetVARIANT())))
        return { nullptr };

    if (V_VT(&value) == VT_EMPTY)
        return { nullptr };

    ASSERT(V_VT(&value) == VT_UNKNOWN);

    if (V_VT(&value) != VT_UNKNOWN)
        return { nullptr };

    COMPtr<IAccessible> titleElement(Query, value.punkVal);

    return titleElement;
}

AccessibilityUIElement AccessibilityUIElement::parentElement()
{
    if (!m_element)
        return { nullptr };

    COMPtr<IDispatch> parent;
    m_element->get_accParent(&parent);

    COMPtr<IAccessible> parentAccessible(Query, parent);
    return parentAccessible;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfChildren()
{
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::parameterizedAttributeNames()
{
    return createEmptyJSString();
}

static VARIANT& self()
{
    static _variant_t vSelf;
    static bool haveInitialized;

    if (!haveInitialized) {
        V_VT(&vSelf) = VT_I4;
        V_I4(&vSelf) = CHILDID_SELF;
    }
    return vSelf;
}

static _bstr_t convertToDRTLabel(const _bstr_t roleName)
{
    if (!wcscmp(roleName, L"cell"))
        return _bstr_t(L"AXCell");
    if (!wcscmp(roleName, L"check box"))
        return _bstr_t(L"AXCheckBox");
    if (!wcscmp(roleName, L"client"))
        return _bstr_t(L"AXWebArea");
    if (!wcscmp(roleName, L"column"))
        return _bstr_t(L"AXColumn");
    if (!wcscmp(roleName, L"column header"))
        return _bstr_t(L"AXCell");
    if (!wcscmp(roleName, L"combo box"))
        return _bstr_t(L"AXComboBox");
    if (!wcscmp(roleName, L"grouping"))
        return _bstr_t(L"AXGroup");
    if (!wcscmp(roleName, L"editable text"))
        return _bstr_t(L"AXStaticText"); // Might be AXTextField, too.
    if (!wcscmp(roleName, L"graphic"))
        return _bstr_t(L"AXImage");
    if (!wcscmp(roleName, L"link"))
        return _bstr_t(L"AXLink");
    if (!wcscmp(roleName, L"list item"))
        return _bstr_t(L"AXTab");
    if (!wcscmp(roleName, L"list"))
        return _bstr_t(L"AXList");
    if (!wcscmp(roleName, L"menu bar"))
        return _bstr_t(L"AXMenuBar");
    if (!wcscmp(roleName, L"page tab list"))
        return _bstr_t(L"AXTabGroup");
    if (!wcscmp(roleName, L"page tab"))
        return _bstr_t(L"AXTab");
    if (!wcscmp(roleName, L"push button"))
        return _bstr_t(L"AXButton");
    if (!wcscmp(roleName, L"progress bar"))
        return _bstr_t(L"AXProgressIndicator");
    if (!wcscmp(roleName, L"radio button"))
        return _bstr_t(L"AXRadioButton");
    if (!wcscmp(roleName, L"row"))
        return _bstr_t(L"AXRow");
    if (!wcscmp(roleName, L"table"))
        return _bstr_t(L"AXTable");
    if (!wcscmp(roleName, L"text"))
        return _bstr_t(L"AXStaticText");

    return roleName;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::role()
{
    if (!m_element)
        return adopt(JSStringCreateWithUTF8CString("AXRole: "));

    _variant_t vRole;
    if (FAILED(m_element->get_accRole(self(), &vRole.GetVARIANT())))
        return adopt(JSStringCreateWithUTF8CString("AXRole: "));

    ASSERT(V_VT(&vRole) == VT_I4 || V_VT(&vRole) == VT_BSTR);

    _bstr_t result;
    if (V_VT(&vRole) == VT_I4) {
        unsigned roleTextLength = ::GetRoleText(V_I4(&vRole), nullptr, 0) + 1;

        Vector<TCHAR> roleText(roleTextLength);

        ::GetRoleText(V_I4(&vRole), roleText.data(), roleTextLength);

        result = roleText.data();
    } else if (V_VT(&vRole) == VT_BSTR)
        result = V_BSTR(&vRole);

    return adopt(JSStringCreateWithBSTR(_bstr_t(L"AXRole: ") + convertToDRTLabel(result)));
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::subrole()
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::roleDescription()
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::computedRoleString()
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::title()
{
    if (!m_element)
        return adopt(JSStringCreateWithUTF8CString("AXTitle: "));

    _bstr_t titleBSTR;
    if (FAILED(m_element->get_accName(self(), &titleBSTR.GetBSTR())))
        return adopt(JSStringCreateWithUTF8CString("AXTitle: "));

    return adopt(JSStringCreateWithBSTR(_bstr_t(L"AXTitle: ") + titleBSTR));
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::description()
{
    if (!m_element)
        return adopt(JSStringCreateWithUTF8CString("AXDescription: "));

    _bstr_t descriptionBSTR;
    if (FAILED(m_element->get_accDescription(self(), &descriptionBSTR.GetBSTR())))
        return adopt(JSStringCreateWithUTF8CString("AXDescription: "));

    if (!descriptionBSTR.length())
        return adopt(JSStringCreateWithUTF8CString("AXDescription: "));

    if (wcsstr(static_cast<wchar_t*>(descriptionBSTR), L"Description: ") == static_cast<wchar_t*>(descriptionBSTR)) {
        // The Mozilla MSAA implementation requires that the string returned to us be prefixed with "Description: "
        // To match the Mac test results, we will just prefix with AX -> AXDescription:
        return adopt(JSStringCreateWithBSTR(_bstr_t(L"AX") + descriptionBSTR));
    }

    return adopt(JSStringCreateWithBSTR(descriptionBSTR));
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringValue()
{
    if (!m_element)
        return adopt(JSStringCreateWithUTF8CString("AXValue: "));

    _bstr_t valueBSTR;
    if (FAILED(m_element->get_accValue(self(), &valueBSTR.GetBSTR())))
        return adopt(JSStringCreateWithUTF8CString("AXValue: "));

    return adopt(JSStringCreateWithBSTR(_bstr_t(L"AXValue: ") + valueBSTR));
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::language()
{
    if (!m_element)
        return adopt(JSStringCreateWithUTF8CString("AXLanguage: "));

    COMPtr<IAccessibleComparable> accessible2Element = comparableObject(m_element.get());
    if (!accessible2Element)
        return adopt(JSStringCreateWithUTF8CString("AXLanguage: "));

    IA2Locale locale;
    if (FAILED(accessible2Element->get_locale(&locale)))
        return adopt(JSStringCreateWithUTF8CString("AXLanguage: "));

    return adopt(JSStringCreateWithBSTR(_bstr_t(L"AXLanguage: ") + _bstr_t(locale.language, false)));
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::helpText() const
{
    if (!m_element)
        return adopt(JSStringCreateWithUTF8CString("AXHelp: "));

    _bstr_t helpTextBSTR;
    if (FAILED(m_element->get_accHelp(self(), &helpTextBSTR.GetBSTR())))
        return adopt(JSStringCreateWithUTF8CString("AXHelp: "));

    return adopt(JSStringCreateWithBSTR(_bstr_t(L"AXHelp: ") + helpTextBSTR));
}

double AccessibilityUIElement::x()
{
    if (!m_element)
        return 0;

    long x, y, width, height;
    if (FAILED(m_element->accLocation(&x, &y, &width, &height, self())))
        return 0;
    return x;
}

double AccessibilityUIElement::y()
{
    if (!m_element)
        return 0;

    long x, y, width, height;
    if (FAILED(m_element->accLocation(&x, &y, &width, &height, self())))
        return 0;
    return y;
}

double AccessibilityUIElement::width()
{
    if (!m_element)
        return 0;

    long x, y, width, height;
    if (FAILED(m_element->accLocation(&x, &y, &width, &height, self())))
        return 0;
    return width;
}

double AccessibilityUIElement::height()
{
    if (!m_element)
        return 0;

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

JSRetainPtr<JSStringRef> AccessibilityUIElement::valueDescription()
{
    return nullptr;
}

static DWORD accessibilityState(COMPtr<IAccessible> element)
{
    _variant_t state;
    if (FAILED(element->get_accState(self(), &state.GetVARIANT())))
        return 0;

    ASSERT(V_VT(&state) == VT_I4);

    DWORD result = state.lVal;

    return result;
}

bool AccessibilityUIElement::isFocused() const
{
    DWORD state = accessibilityState(m_element);
    return (state & STATE_SYSTEM_FOCUSED) == STATE_SYSTEM_FOCUSED;
}

bool AccessibilityUIElement::isSelected() const
{
    DWORD state = accessibilityState(m_element);
    return (state & STATE_SYSTEM_SELECTED) == STATE_SYSTEM_SELECTED;
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
    return 0;
}

bool AccessibilityUIElement::isExpanded() const
{
    return false;
}

bool AccessibilityUIElement::isChecked() const
{
    if (!m_element)
        return false;

    _variant_t vState;
    if (FAILED(m_element->get_accState(self(), &vState.GetVARIANT())))
        return false;

    return vState.lVal & STATE_SYSTEM_CHECKED;
}

bool AccessibilityUIElement::isIndeterminate() const
{
    // FIXME: implement
    return false;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::orientation() const
{
    return 0;
}

double AccessibilityUIElement::intValue() const
{
    if (!m_element)
        return 0;

    _bstr_t valueBSTR;
    if (FAILED(m_element->get_accValue(self(), &valueBSTR.GetBSTR())) || !valueBSTR.length())
        return 0;

    TCHAR* ignored;
    return _tcstod(static_cast<TCHAR*>(valueBSTR), &ignored);
}

double AccessibilityUIElement::minValue()
{
    return 0;
}

double AccessibilityUIElement::maxValue()
{
    return 0;
}

bool AccessibilityUIElement::isPressActionSupported()
{
    if (!m_element)
        return 0;

    _bstr_t valueBSTR;
    if (FAILED(m_element->get_accDefaultAction(self(), &valueBSTR.GetBSTR())))
        return false;

    if (!valueBSTR.length())
        return false;

    return true;
}

bool AccessibilityUIElement::isIncrementActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isDecrementActionSupported()
{
    return false;
}

bool AccessibilityUIElement::isEnabled()
{
    DWORD state = accessibilityState(m_element);
    return (state & STATE_SYSTEM_UNAVAILABLE) != STATE_SYSTEM_UNAVAILABLE;
}

bool AccessibilityUIElement::isRequired() const
{
    return false;
}


int AccessibilityUIElement::insertionPointLineNumber()
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumnHeaders()
{
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRowHeaders()
{
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfColumns()
{
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfRows()
{
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfVisibleCells()
{
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributesOfHeader()
{
    return createEmptyJSString();
}

int AccessibilityUIElement::indexInTable()
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::rowIndexRange()
{
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::columnIndexRange()
{
    return createEmptyJSString();
}

int AccessibilityUIElement::lineForIndex(int)
{
    return 0;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::boundsForRange(unsigned location, unsigned length)
{
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringForRange(unsigned, unsigned)
{
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::attributedStringForRange(unsigned, unsigned)
{
    return createEmptyJSString();
}

bool AccessibilityUIElement::attributedStringRangeIsMisspelled(unsigned, unsigned)
{
    return false;
}

unsigned AccessibilityUIElement::uiElementCountForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::uiElementForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    return { nullptr };
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectTextWithCriteria(JSContextRef context, JSStringRef ambiguityResolution, JSValueRef searchStrings, JSStringRef replacementString, JSStringRef activity)
{
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::cellForColumnAndRow(unsigned column, unsigned row)
{
    return { nullptr };
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::selectedTextRange()
{
    COMPtr<IAccessibleComparable> comparable = comparableObject(platformUIElement().get());
    if (!comparable)
        return createEmptyJSString();

    _variant_t value;
    if (FAILED(comparable->get_attribute(_bstr_t(L"AXSelectedTextRangeAttribute"), &value.GetVARIANT())))
        return createEmptyJSString();

    ASSERT(V_VT(&value) == VT_BSTR);
    return adopt(JSStringCreateWithBSTR(value.bstrVal));
}

void AccessibilityUIElement::setSelectedTextRange(unsigned location, unsigned length)
{
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::stringAttributeValue(JSStringRef attribute)
{
    // FIXME: implement
    return createEmptyJSString();
}

double AccessibilityUIElement::numberAttributeValue(JSStringRef attribute)
{
    // FIXME: implement
    return 0;
}

bool AccessibilityUIElement::boolAttributeValue(JSStringRef attribute)
{
    // FIXME: implement
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

void AccessibilityUIElement::increment()
{
}

void AccessibilityUIElement::decrement()
{
}

void AccessibilityUIElement::showMenu()
{
    if (!m_element)
        return;

    ASSERT(hasPopup());
    m_element->accDoDefaultAction(self());
}

void AccessibilityUIElement::press()
{
    if (!m_element)
        return;

    m_element->accDoDefaultAction(self());
}

AccessibilityUIElement AccessibilityUIElement::disclosedRowAtIndex(unsigned index)
{
    return { nullptr };
}

AccessibilityUIElement AccessibilityUIElement::ariaOwnsElementAtIndex(unsigned index)
{
    return { nullptr };
}

AccessibilityUIElement AccessibilityUIElement::ariaFlowToElementAtIndex(unsigned index)
{
    return { nullptr };
}

AccessibilityUIElement AccessibilityUIElement::ariaControlsElementAtIndex(unsigned index)
{
    return { nullptr };
}

AccessibilityUIElement AccessibilityUIElement::selectedRowAtIndex(unsigned index)
{
    return { nullptr };
}

AccessibilityUIElement AccessibilityUIElement::rowAtIndex(unsigned index)
{
    return { nullptr };
}

AccessibilityUIElement AccessibilityUIElement::disclosedByRow()
{
    return { nullptr };
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::accessibilityValue() const
{
    if (!m_element)
        return createEmptyJSString();

    _bstr_t valueBSTR;
    if (FAILED(m_element->get_accValue(self(), &valueBSTR.GetBSTR())) || !valueBSTR.length())
        return createEmptyJSString();

    return adopt(JSStringCreateWithBSTR(valueBSTR));
}

void AccessibilityUIElement::clearSelectedChildren() const
{
    // FIXME: implement
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentEncoding()
{
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::documentURI()
{
    return createEmptyJSString();
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::url()
{
    // FIXME: implement
    return createEmptyJSString();
}

bool AccessibilityUIElement::addNotificationListener(JSObjectRef functionCallback)
{
    if (!functionCallback)
        return false;

    sharedFrameLoadDelegate->accessibilityController()->winAddNotificationListener(m_element, functionCallback);
    return true;
}

void AccessibilityUIElement::removeNotificationListener()
{
    // FIXME: implement
}

bool AccessibilityUIElement::isFocusable() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isSelectable() const
{
    DWORD state = accessibilityState(m_element);
    return (state & STATE_SYSTEM_SELECTABLE) == STATE_SYSTEM_SELECTABLE;
}

bool AccessibilityUIElement::isMultiSelectable() const
{
    DWORD multiSelectable = STATE_SYSTEM_EXTSELECTABLE | STATE_SYSTEM_MULTISELECTABLE;
    DWORD state = accessibilityState(m_element);
    return (state & multiSelectable) == multiSelectable;
}

bool AccessibilityUIElement::isSelectedOptionActive() const
{
    // FIXME: implement
    return false;
}

bool AccessibilityUIElement::isVisible() const
{
    DWORD state = accessibilityState(m_element);
    return (state & STATE_SYSTEM_INVISIBLE) != STATE_SYSTEM_INVISIBLE;
}

bool AccessibilityUIElement::isOffScreen() const
{
    DWORD state = accessibilityState(m_element);
    return (state & STATE_SYSTEM_OFFSCREEN) == STATE_SYSTEM_OFFSCREEN;
}

bool AccessibilityUIElement::isCollapsed() const
{
    DWORD state = accessibilityState(m_element);
    return (state & STATE_SYSTEM_COLLAPSED) == STATE_SYSTEM_COLLAPSED;
}

bool AccessibilityUIElement::isIgnored() const
{
    // FIXME: implement
    return false;
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
    DWORD state = accessibilityState(m_element);
    return (state & STATE_SYSTEM_HASPOPUP) == STATE_SYSTEM_HASPOPUP;
}

JSRetainPtr<JSStringRef> AccessibilityUIElement::popupValue() const
{
    return createEmptyJSString();
}

void AccessibilityUIElement::takeFocus()
{
    if (!m_element)
        return;

    m_element->accSelect(SELFLAG_TAKEFOCUS, self());
}

void AccessibilityUIElement::takeSelection()
{
    if (!m_element)
        return;

    m_element->accSelect(SELFLAG_TAKESELECTION, self());
}

void AccessibilityUIElement::addSelection()
{
    if (!m_element)
        return;

    m_element->accSelect(SELFLAG_ADDSELECTION, self());
}

void AccessibilityUIElement::removeSelection()
{
    if (!m_element)
        return;

    m_element->accSelect(SELFLAG_REMOVESELECTION, self());
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

JSRetainPtr<JSStringRef> AccessibilityUIElement::classList() const
{
    // FIXME: implement
    return 0;
}

unsigned AccessibilityUIElement::selectedChildrenCount() const
{
    // FIXME: implement
    return 0;
}

AccessibilityUIElement AccessibilityUIElement::selectedChildAtIndex(unsigned) const
{
    // FIXME: implement
    return { nullptr };
}

void AccessibilityUIElement::columnHeaders(Vector<AccessibilityUIElement>&) const
{
    // FIXME: implement
}

void AccessibilityUIElement::rowHeaders(Vector<AccessibilityUIElement>&) const
{
    // FIXME: implement
}
