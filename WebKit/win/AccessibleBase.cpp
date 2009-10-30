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
#include "WebKitDLL.h"
#include "AccessibleBase.h"

#include "AccessibleImage.h"
#include "WebView.h"
#include <WebCore/AccessibilityObject.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/BString.h>
#include <WebCore/Element.h>
#include <WebCore/EventHandler.h>
#include <WebCore/FrameView.h>
#include <WebCore/HostWindow.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLFrameElementBase.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/IntRect.h>
#include <WebCore/PlatformKeyboardEvent.h>
#include <WebCore/RenderFrame.h>
#include <WebCore/RenderObject.h>
#include <WebCore/RenderView.h>
#include <oleacc.h>
#include <wtf/RefPtr.h>

using namespace WebCore;

AccessibleBase::AccessibleBase(AccessibilityObject* obj)
    : AccessibilityObjectWrapper(obj)
    , m_refCount(0)
{
    ASSERT_ARG(obj, obj);
    m_object->setWrapper(this);
    ++gClassCount;
    gClassNameCount.add("AccessibleBase");
}

AccessibleBase::~AccessibleBase()
{
    --gClassCount;
    gClassNameCount.remove("AccessibleBase");
}

AccessibleBase* AccessibleBase::createInstance(AccessibilityObject* obj)
{
    ASSERT_ARG(obj, obj);

    if (obj->isImage())
        return new AccessibleImage(obj);

    return new AccessibleBase(obj);
}

// IUnknown
HRESULT STDMETHODCALLTYPE AccessibleBase::QueryInterface(REFIID riid, void** ppvObject)
{
    if (IsEqualGUID(riid, __uuidof(IAccessible)))
        *ppvObject = this;
    else if (IsEqualGUID(riid, __uuidof(IDispatch)))
        *ppvObject = this;
    else if (IsEqualGUID(riid, __uuidof(IUnknown)))
        *ppvObject = this;
    else {
        *ppvObject = 0;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG STDMETHODCALLTYPE AccessibleBase::Release(void)
{
    ASSERT(m_refCount > 0);
    if (--m_refCount)
        return m_refCount;
    delete this;
    return 0;
}

// IAccessible
HRESULT STDMETHODCALLTYPE AccessibleBase::get_accParent(IDispatch** parent)
{
    *parent = 0;

    if (!m_object || !m_object->topDocumentFrameView())
        return E_FAIL;

    return WebView::AccessibleObjectFromWindow(m_object->topDocumentFrameView()->hostWindow()->platformPageClient(),
        OBJID_WINDOW, __uuidof(IAccessible), reinterpret_cast<void**>(parent));
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accChildCount(long* count)
{
    if (!m_object)
        return E_FAIL;
    if (!count)
        return E_POINTER;
    *count = static_cast<long>(m_object->children().size());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accChild(VARIANT vChild, IDispatch** ppChild)
{
    if (!ppChild)
        return E_POINTER;

    *ppChild = 0;

    AccessibilityObject* childObj;

    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);
    if (FAILED(hr))
        return hr;

    *ppChild = static_cast<IDispatch*>(wrapper(childObj));
    (*ppChild)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accName(VARIANT vChild, BSTR* name)
{
    if (!name)
        return E_POINTER;

    *name = 0;

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    if (*name = BString(wrapper(childObj)->name()).release())
        return S_OK;
    return S_FALSE;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accValue(VARIANT vChild, BSTR* value)
{
    if (!value)
        return E_POINTER;

    *value = 0;

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    if (*value = BString(wrapper(childObj)->value()).release())
        return S_OK;
    return S_FALSE;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accDescription(VARIANT vChild, BSTR* description)
{
    if (!description)
        return E_POINTER;

    *description = 0;

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    // TODO: Description, for SELECT subitems, should be a string describing
    // the position of the item in its group and of the group in the list (see
    // Firefox).
    if (*description = BString(wrapper(childObj)->description()).release())
        return S_OK;
    return S_FALSE;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accRole(VARIANT vChild, VARIANT* pvRole)
{
    if (!pvRole)
        return E_POINTER;

    ::VariantInit(pvRole);

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    pvRole->vt = VT_I4;
    pvRole->lVal = wrapper(childObj)->role();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accState(VARIANT vChild, VARIANT* pvState)
{
    if (!pvState)
        return E_POINTER;

    ::VariantInit(pvState);

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    pvState->vt = VT_I4;
    pvState->lVal = 0;

    if (childObj->isLinked())
        pvState->lVal |= STATE_SYSTEM_LINKED;

    if (childObj->isHovered())
        pvState->lVal |= STATE_SYSTEM_HOTTRACKED;

    if (!childObj->isEnabled())
        pvState->lVal |= STATE_SYSTEM_UNAVAILABLE;

    if (childObj->isReadOnly())
        pvState->lVal |= STATE_SYSTEM_READONLY;

    if (childObj->isOffScreen())
        pvState->lVal |= STATE_SYSTEM_OFFSCREEN;

    if (childObj->isMultiSelect())
        pvState->lVal |= STATE_SYSTEM_MULTISELECTABLE;

    if (childObj->isPasswordField())
        pvState->lVal |= STATE_SYSTEM_PROTECTED;

    if (childObj->isIndeterminate())
        pvState->lVal |= STATE_SYSTEM_INDETERMINATE;

    if (childObj->isChecked())
        pvState->lVal |= STATE_SYSTEM_CHECKED;

    if (childObj->isPressed())
        pvState->lVal |= STATE_SYSTEM_PRESSED;

    if (childObj->isFocused())
        pvState->lVal |= STATE_SYSTEM_FOCUSED;

    if (childObj->isVisited())
        pvState->lVal |= STATE_SYSTEM_TRAVERSED;

    if (childObj->canSetFocusAttribute())
        pvState->lVal |= STATE_SYSTEM_FOCUSABLE;

    // TODO: Add selected and selectable states.

    return S_OK;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accHelp(VARIANT vChild, BSTR* helpText)
{
    if (!helpText)
        return E_POINTER;

    *helpText = 0;

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    if (*helpText = BString(childObj->helpText()).release())
        return S_OK;
    return S_FALSE;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accKeyboardShortcut(VARIANT vChild, BSTR* shortcut)
{
    if (!shortcut)
        return E_POINTER;

    *shortcut = 0;

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    String accessKey = childObj->accessKey();
    if (accessKey.isNull())
        return S_FALSE;

    static String accessKeyModifiers;
    if (accessKeyModifiers.isNull()) {
        unsigned modifiers = EventHandler::accessKeyModifiers();
        // Follow the same order as Mozilla MSAA implementation:
        // Ctrl+Alt+Shift+Meta+key. MSDN states that keyboard shortcut strings
        // should not be localized and defines the separator as "+".
        if (modifiers & PlatformKeyboardEvent::CtrlKey)
            accessKeyModifiers += "Ctrl+";
        if (modifiers & PlatformKeyboardEvent::AltKey)
            accessKeyModifiers += "Alt+";
        if (modifiers & PlatformKeyboardEvent::ShiftKey)
            accessKeyModifiers += "Shift+";
        if (modifiers & PlatformKeyboardEvent::MetaKey)
            accessKeyModifiers += "Win+";
    }
    *shortcut = BString(accessKeyModifiers + accessKey).release();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::accSelect(long, VARIANT)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accSelection(VARIANT*)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accFocus(VARIANT* pvFocusedChild)
{
    if (!pvFocusedChild)
        return E_POINTER;

    ::VariantInit(pvFocusedChild);

    if (!m_object)
        return E_FAIL;

    AccessibilityObject* focusedObj = m_object->focusedUIElement();
    if (!focusedObj)
        return S_FALSE;

    if (focusedObj == m_object) {
        V_VT(pvFocusedChild) = VT_I4;
        V_I4(pvFocusedChild) = CHILDID_SELF;
        return S_OK;
    }

    V_VT(pvFocusedChild) = VT_DISPATCH;
    V_DISPATCH(pvFocusedChild) = wrapper(focusedObj);
    V_DISPATCH(pvFocusedChild)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::get_accDefaultAction(VARIANT vChild, BSTR* action)
{
    if (!action)
        return E_POINTER;

    *action = 0;

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    if (*action = BString(childObj->actionVerb()).release())
        return S_OK;
    return S_FALSE;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::accLocation(long* left, long* top, long* width, long* height, VARIANT vChild)
{
    if (!left || !top || !width || !height)
        return E_POINTER;

    *left = *top = *width = *height = 0;

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    if (!childObj->documentFrameView())
        return E_FAIL;

    IntRect screenRect(childObj->documentFrameView()->contentsToScreen(childObj->boundingBoxRect()));
    *left = screenRect.x();
    *top = screenRect.y();
    *width = screenRect.width();
    *height = screenRect.height();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::accNavigate(long direction, VARIANT vFromChild, VARIANT* pvNavigatedTo)
{
    if (!pvNavigatedTo)
        return E_POINTER;

    ::VariantInit(pvNavigatedTo);

    AccessibilityObject* childObj = 0;

    switch (direction) {
        case NAVDIR_DOWN:
        case NAVDIR_UP:
        case NAVDIR_LEFT:
        case NAVDIR_RIGHT:
            // These directions are not implemented, matching Mozilla and IE.
            return E_NOTIMPL;
        case NAVDIR_LASTCHILD:
        case NAVDIR_FIRSTCHILD:
            // MSDN states that navigating to first/last child can only be from self.
            if (vFromChild.lVal != CHILDID_SELF)
                return E_INVALIDARG;

            if (!m_object)
                return E_FAIL;

            if (direction == NAVDIR_FIRSTCHILD)
                childObj = m_object->firstChild();
            else
                childObj = m_object->lastChild();
            break;
        case NAVDIR_NEXT:
        case NAVDIR_PREVIOUS: {
            // Navigating to next and previous is allowed from self or any of our children.
            HRESULT hr = getAccessibilityObjectForChild(vFromChild, childObj);
            if (FAILED(hr))
                return hr;

            if (direction == NAVDIR_NEXT)
                childObj = childObj->nextSibling();
            else
                childObj = childObj->previousSibling();
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            return E_INVALIDARG;
    }

    if (!childObj)
        return S_FALSE;

    V_VT(pvNavigatedTo) = VT_DISPATCH;
    V_DISPATCH(pvNavigatedTo) = wrapper(childObj);
    V_DISPATCH(pvNavigatedTo)->AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::accHitTest(long x, long y, VARIANT* pvChildAtPoint)
{
    if (!pvChildAtPoint)
        return E_POINTER;

    ::VariantInit(pvChildAtPoint);

    if (!m_object || !m_object->documentFrameView())
        return E_FAIL;

    IntPoint point = m_object->documentFrameView()->screenToContents(IntPoint(x, y));
    AccessibilityObject* childObj = m_object->doAccessibilityHitTest(point);

    if (!childObj) {
        // If we did not hit any child objects, test whether the point hit us, and
        // report that.
        if (!m_object->boundingBoxRect().contains(point))
            return S_FALSE;
        childObj = m_object;
    }

    if (childObj == m_object) {
        V_VT(pvChildAtPoint) = VT_I4;
        V_I4(pvChildAtPoint) = CHILDID_SELF;
    } else {
        V_VT(pvChildAtPoint) = VT_DISPATCH;
        V_DISPATCH(pvChildAtPoint) = static_cast<IDispatch*>(wrapper(childObj));
        V_DISPATCH(pvChildAtPoint)->AddRef();
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE AccessibleBase::accDoDefaultAction(VARIANT vChild)
{
    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    if (!childObj->performDefaultAction())
        return S_FALSE;

    return S_OK;
}

// AccessibleBase
String AccessibleBase::name() const
{
    return m_object->nameForMSAA();
}

String AccessibleBase::value() const
{
    return m_object->stringValueForMSAA();
}

String AccessibleBase::description() const
{
    String desc = m_object->accessibilityDescription();
    if (desc.isNull())
        return desc;

    // From the Mozilla MSAA implementation:
    // "Signal to screen readers that this description is speakable and is not
    // a formatted positional information description. Don't localize the
    // 'Description: ' part of this string, it will be parsed out by assistive
    // technologies."
    return "Description: " + desc;
}

static long MSAARole(AccessibilityRole role)
{
    switch (role) {
        case WebCore::ButtonRole:
            return ROLE_SYSTEM_PUSHBUTTON;
        case WebCore::RadioButtonRole:
            return ROLE_SYSTEM_RADIOBUTTON;
        case WebCore::CheckBoxRole:
            return ROLE_SYSTEM_CHECKBUTTON;
        case WebCore::SliderRole:
            return ROLE_SYSTEM_SLIDER;
        case WebCore::TabGroupRole:
            return ROLE_SYSTEM_PAGETABLIST;
        case WebCore::TextFieldRole:
        case WebCore::TextAreaRole:
        case WebCore::ListMarkerRole:
            return ROLE_SYSTEM_TEXT;
        case WebCore::StaticTextRole:
            return ROLE_SYSTEM_STATICTEXT;
        case WebCore::OutlineRole:
            return ROLE_SYSTEM_OUTLINE;
        case WebCore::ColumnRole:
            return ROLE_SYSTEM_COLUMN;
        case WebCore::RowRole:
            return ROLE_SYSTEM_ROW;
        case WebCore::GroupRole:
            return ROLE_SYSTEM_GROUPING;
        case WebCore::ListRole:
            return ROLE_SYSTEM_LIST;
        case WebCore::TableRole:
            return ROLE_SYSTEM_TABLE;
        case WebCore::LinkRole:
        case WebCore::WebCoreLinkRole:
            return ROLE_SYSTEM_LINK;
        case WebCore::ImageMapRole:
        case WebCore::ImageRole:
            return ROLE_SYSTEM_GRAPHIC;
        default:
            // This is the default role for MSAA.
            return ROLE_SYSTEM_CLIENT;
    }
}

long AccessibleBase::role() const
{
    return MSAARole(m_object->roleValue());
}

HRESULT AccessibleBase::getAccessibilityObjectForChild(VARIANT vChild, AccessibilityObject*& childObj) const
{
    childObj = 0;

    if (!m_object)
        return E_FAIL;

    if (vChild.vt != VT_I4)
        return E_INVALIDARG;

    if (vChild.lVal == CHILDID_SELF)
        childObj = m_object;
    else if (vChild.lVal < 0) {
        // When broadcasting MSAA events, we negate the AXID and pass it as the
        // child ID.
        Document* document = m_object->document();
        if (!document)
            return E_FAIL;

        childObj = document->axObjectCache()->objectFromAXID(-vChild.lVal);
    } else {
        size_t childIndex = static_cast<size_t>(vChild.lVal - 1);

        if (childIndex >= m_object->children().size())
            return E_FAIL;
        childObj = m_object->children().at(childIndex).get();
    }

    if (!childObj)
        return E_FAIL;

    return S_OK;
}

AccessibleBase* AccessibleBase::wrapper(AccessibilityObject* obj)
{
    AccessibleBase* result = static_cast<AccessibleBase*>(obj->wrapper());
    if (!result)
        result = createInstance(obj);
    return result;
}
