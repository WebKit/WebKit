/*
 * Copyright (C) 2008-2010, 2013, 2015 Apple Inc. All Rights Reserved.
 * Copyright (C) 2012 Serotek Corporation. All Rights Reserved.
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

#include "WebKitDLL.h"
#include "AccessibleBase.h"

#include "AccessibleImage.h"
#include "AccessibleTextImpl.h"
#include "WebView.h"
#include <WebCore/AccessibilityListBox.h>
#include <WebCore/AccessibilityMenuListPopup.h>
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
#include <WebCore/NotImplemented.h>
#include <WebCore/PlatformEvent.h>
#include <WebCore/RenderFrame.h>
#include <WebCore/RenderObject.h>
#include <WebCore/RenderView.h>
#include <comutil.h>
#include <oleacc.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringBuilder.h>

using namespace WebCore;

AccessibleBase::AccessibleBase(AccessibilityObject* obj, HWND window)
    : AccessibilityObjectWrapper(obj)
    , m_window(window)
{
    ASSERT_ARG(obj, obj);
    m_object->setWrapper(this);
    ++gClassCount;
    gClassNameCount().add("AccessibleBase");
}

AccessibleBase::~AccessibleBase()
{
    --gClassCount;
    gClassNameCount().remove("AccessibleBase");
}

AccessibleBase* AccessibleBase::createInstance(AccessibilityObject* obj, HWND window)
{
    ASSERT_ARG(obj, obj);

    if (obj->isImage())
        return new AccessibleImage(obj, window);
    else if (obj->isStaticText() || obj->isTextControl() || (obj->node() && obj->node()->isTextNode()))
        return new AccessibleText(obj, window);

    return new AccessibleBase(obj, window);
}

HRESULT AccessibleBase::QueryService(_In_ REFGUID guidService, _In_ REFIID riid, _COM_Outptr_ void **ppvObject)
{
    if (!IsEqualGUID(guidService, SID_AccessibleComparable)
        && !IsEqualGUID(guidService, IID_IAccessible2_2)
        && !IsEqualGUID(guidService, IID_IAccessible2)
        && !IsEqualGUID(guidService, IID_IAccessibleApplication)
        && !IsEqualGUID(guidService, IID_IAccessible)
        && !IsEqualGUID(guidService, IID_IAccessibleText)
        && !IsEqualGUID(guidService, IID_IAccessibleText2)
        && !IsEqualGUID(guidService, IID_IAccessibleEditableText)) {
        *ppvObject = nullptr;
        return E_INVALIDARG;
    }
    return QueryInterface(riid, ppvObject);
}

// IUnknown
HRESULT AccessibleBase::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    if (IsEqualGUID(riid, __uuidof(IAccessible)))
        *ppvObject = static_cast<IAccessible*>(this);
    else if (IsEqualGUID(riid, __uuidof(IDispatch)))
        *ppvObject = static_cast<IAccessible*>(this);
    else if (IsEqualGUID(riid, __uuidof(IUnknown)))
        *ppvObject = static_cast<IAccessible*>(this);
    else if (IsEqualGUID(riid, __uuidof(IAccessible2_2)))
        *ppvObject = static_cast<IAccessible2_2*>(this);
    else if (IsEqualGUID(riid, __uuidof(IAccessible2)))
        *ppvObject = static_cast<IAccessible2*>(this);
    else if (IsEqualGUID(riid, __uuidof(IAccessibleComparable)))
        *ppvObject = static_cast<IAccessibleComparable*>(this);
    else if (IsEqualGUID(riid, __uuidof(IServiceProvider)))
        *ppvObject = static_cast<IServiceProvider*>(this);
    else if (IsEqualGUID(riid, __uuidof(AccessibleBase)))
        *ppvObject = static_cast<AccessibleBase*>(this);
    else {
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

ULONG AccessibleBase::Release()
{
    ASSERT(m_refCount > 0);
    if (--m_refCount)
        return m_refCount;
    delete this;
    return 0;
}

// IAccessible2_2
HRESULT AccessibleBase::get_attribute(_In_ BSTR key, _Out_ VARIANT* value)
{
    if (!value)
        return E_POINTER;

    AtomString keyAtomic(key, ::SysStringLen(key));

    accessibilityAttributeValue(keyAtomic, value);

    return S_OK;
}

HRESULT AccessibleBase::get_accessibleWithCaret(_COM_Outptr_opt_ IUnknown** accessible, _Out_ long* caretOffset)
{
    if (!accessible || !caretOffset)
        return E_POINTER;
    *accessible = nullptr;
    *caretOffset = 0;
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessibleBase::get_relationTargetsOfType(_In_ BSTR type, long maxTargets, __deref_out_ecount_full_opt(*nTargets) IUnknown*** targets, _Out_ long* nTargets)
{
    notImplemented();
    return E_NOTIMPL;
}

// IAccessible2
HRESULT AccessibleBase::get_nRelations(_Out_ long* nRelations)
{
    if (!nRelations)
        return E_POINTER;

    if (!m_object)
        return E_FAIL;
    notImplemented();
    *nRelations = 0;
    return S_OK;
}

HRESULT AccessibleBase::get_relation(long relationIndex, _COM_Outptr_opt_ IAccessibleRelation** relation)
{
    if (!relation)
        return E_POINTER;

    *relation = nullptr;

    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessibleBase::get_relations(long maxRelations, __out_ecount_part(maxRelations, *nRelations)  IAccessibleRelation** relations, _Out_ long* nRelations)
{
    if (!relations || !nRelations)
        return E_POINTER;

    *relations = nullptr;

    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessibleBase::role(_Out_ long* role)
{
    if (!role)
        return E_POINTER;
    *role = 0;
    if (!m_object)
        return E_FAIL;

    *role = wrapper(m_object)->role();
    return S_OK;
}

HRESULT AccessibleBase::scrollTo(IA2ScrollType scrollType)
{
    if (!m_object)
        return E_FAIL;
    return S_FALSE;
}

HRESULT AccessibleBase::scrollToPoint(IA2CoordinateType coordinateType, long x, long y)
{
    if (!m_object)
        return E_FAIL;
    return S_FALSE;
}

HRESULT AccessibleBase::get_groupPosition(_Out_ long* groupLevel, _Out_ long* similarItemsInGroup, _Out_ long* positionInGroup)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessibleBase::get_states(_Out_ AccessibleStates* states)
{
    if (!states)
        return E_POINTER;

    if (!m_object)
        return E_FAIL;

    *states = 0;
    notImplemented();
    return S_OK;
}

HRESULT AccessibleBase::get_extendedRole(__deref_opt_out BSTR* extendedRole)
{
    if (!extendedRole)
        return E_POINTER;
    *extendedRole = nullptr;
    if (!m_object)
        return E_FAIL;

    notImplemented();
    return S_FALSE;
}

HRESULT AccessibleBase::get_localizedExtendedRole(__deref_opt_out BSTR* localizedExtendedRole)
{
    if (!localizedExtendedRole)
        return E_POINTER;
    *localizedExtendedRole = nullptr;
    if (!m_object)
        return E_FAIL;

    notImplemented();
    return S_FALSE;
}

HRESULT AccessibleBase::get_nExtendedStates(_Out_ long* nExtendedStates)
{
    if (!nExtendedStates)
        return E_POINTER;

    if (!m_object)
        return E_FAIL;

    notImplemented();
    *nExtendedStates = 0;
    return S_OK;
}

HRESULT AccessibleBase::get_extendedStates(long maxExtendedStates, __deref_out_ecount_part_opt(maxExtendedStates, *nExtendedStates) BSTR** extendedStates, _Out_ long* nExtendedStates)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessibleBase::get_localizedExtendedStates(long maxLocalizedExtendedStates, __deref_out_ecount_part_opt(maxLocalizedExtendedStates, *nLocalizedExtendedStates) BSTR** localizedExtendedStates, _Out_ long* nLocalizedExtendedStates)
{
    notImplemented();
    return E_NOTIMPL;
}

HRESULT AccessibleBase::get_uniqueID(_Out_ long* uniqueID)
{
    if (!uniqueID)
        return E_POINTER;

    if (!m_object)
        return E_FAIL;

    *uniqueID = static_cast<long>(m_object->axObjectID());
    return S_OK;
}

HRESULT AccessibleBase::get_windowHandle(_Out_ HWND* windowHandle)
{
    *windowHandle = m_window;
    return S_OK;
}

HRESULT AccessibleBase::get_indexInParent(_Out_ long* indexInParent)
{
    return E_NOTIMPL;
}

HRESULT AccessibleBase::get_locale(_Out_ IA2Locale* locale)
{
    if (!locale)
        return E_POINTER;

    if (!m_object)
        return E_FAIL;

#if USE(CF)
    locale->language = BString(m_object->language().createCFString().get()).release();
#else
    locale->language = BString(m_object->language()).release();
#endif

    return S_OK;
}

HRESULT AccessibleBase::get_attributes(__deref_opt_out BSTR* attributes)
{
    if (!attributes)
        return E_POINTER;
    *attributes = nullptr;
    if (!m_object)
        return E_FAIL;

    notImplemented();
    return S_FALSE;
}

// IAccessible
HRESULT AccessibleBase::get_accParent(_COM_Outptr_opt_ IDispatch** parent)
{
    if (!parent)
        return E_POINTER;

    *parent = nullptr;

    if (!m_object)
        return E_FAIL;

    AccessibilityObject* parentObject = m_object->parentObjectUnignored();
    if (parentObject) {
        *parent = wrapper(parentObject);
        (*parent)->AddRef();
        return S_OK;
    }

    if (!m_window)
        return E_FAIL;

    return WebView::AccessibleObjectFromWindow(m_window,
        OBJID_WINDOW, __uuidof(IAccessible), reinterpret_cast<void**>(parent));
}

HRESULT AccessibleBase::get_accChildCount(_Out_ long* count)
{
    if (!m_object)
        return E_FAIL;
    if (!count)
        return E_POINTER;
    *count = static_cast<long>(m_object->children().size());
    return S_OK;
}

HRESULT AccessibleBase::get_accChild(VARIANT vChild, _COM_Outptr_opt_ IDispatch** ppChild)
{
    if (!ppChild)
        return E_POINTER;

    *ppChild = nullptr;

    AccessibilityObject* childObj;

    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);
    if (FAILED(hr))
        return hr;

    *ppChild = static_cast<IDispatch*>(wrapper(childObj));
    (*ppChild)->AddRef();
    return S_OK;
}

HRESULT AccessibleBase::get_accName(VARIANT vChild, __deref_opt_out BSTR* name)
{
    if (!name)
        return E_POINTER;

    *name = nullptr;

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    if (*name = BString(wrapper(childObj)->name()).release())
        return S_OK;
    return S_FALSE;
}

HRESULT AccessibleBase::get_accValue(VARIANT vChild, __deref_opt_out BSTR* value)
{
    if (!value)
        return E_POINTER;

    *value = nullptr;

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    if (*value = BString(wrapper(childObj)->value()).release())
        return S_OK;
    return S_FALSE;
}

HRESULT AccessibleBase::get_accDescription(VARIANT vChild, __deref_opt_out BSTR* description)
{
    if (!description)
        return E_POINTER;

    *description = nullptr;

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    if (*description = BString(childObj->descriptionForMSAA()).release())
        return S_OK;

    return S_FALSE;
}

HRESULT AccessibleBase::get_accRole(VARIANT vChild, _Out_ VARIANT* pvRole)
{
    if (!pvRole)
        return E_POINTER;

    ::VariantInit(pvRole);

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    String roleString = childObj->stringRoleForMSAA();
    if (!roleString.isEmpty()) {
        V_VT(pvRole) = VT_BSTR;
        V_BSTR(pvRole) = BString(roleString).release();
        return S_OK;
    }

    pvRole->vt = VT_I4;
    pvRole->lVal = wrapper(childObj)->role();
    return S_OK;
}

long AccessibleBase::state() const
{
    long state = 0;
    if (m_object->isLinked())
        state |= STATE_SYSTEM_LINKED;

    if (m_object->isHovered())
        state |= STATE_SYSTEM_HOTTRACKED;

    if (!m_object->isEnabled())
        state |= STATE_SYSTEM_UNAVAILABLE;

    if (!m_object->canSetValueAttribute())
        state |= STATE_SYSTEM_READONLY;

    if (m_object->isOffScreen())
        state |= STATE_SYSTEM_OFFSCREEN;

    if (m_object->isPasswordField())
        state |= STATE_SYSTEM_PROTECTED;

    if (m_object->isIndeterminate())
        state |= STATE_SYSTEM_INDETERMINATE;

    if (m_object->isChecked())
        state |= STATE_SYSTEM_CHECKED;

    if (m_object->isPressed())
        state |= STATE_SYSTEM_PRESSED;

    if (m_object->isFocused())
        state |= STATE_SYSTEM_FOCUSED;

    if (m_object->isVisited())
        state |= STATE_SYSTEM_TRAVERSED;

    if (m_object->canSetFocusAttribute())
        state |= STATE_SYSTEM_FOCUSABLE;

    if (m_object->isSelected())
        state |= STATE_SYSTEM_SELECTED;

    if (m_object->canSetSelectedAttribute())
        state |= STATE_SYSTEM_SELECTABLE;

    if (m_object->isMultiSelectable())
        state |= STATE_SYSTEM_EXTSELECTABLE | STATE_SYSTEM_MULTISELECTABLE;

    if (!m_object->isVisible())
        state |= STATE_SYSTEM_INVISIBLE;

    if (m_object->isCollapsed())
        state |= STATE_SYSTEM_COLLAPSED;

    if (m_object->roleValue() == AccessibilityRole::PopUpButton) {
        state |= STATE_SYSTEM_HASPOPUP;

        if (!m_object->isCollapsed())
            state |= STATE_SYSTEM_EXPANDED;
    }

    return state;
}

HRESULT AccessibleBase::get_accState(VARIANT vChild, _Out_ VARIANT* pvState)
{
    if (!pvState)
        return E_POINTER;

    ::VariantInit(pvState);

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    pvState->vt = VT_I4;
    pvState->lVal = wrapper(childObj)->state();

    return S_OK;
}

HRESULT AccessibleBase::get_accHelp(VARIANT vChild, __deref_opt_out BSTR* helpText)
{
    if (!helpText)
        return E_POINTER;

    *helpText = nullptr;

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    if (*helpText = BString(childObj->helpText()).release())
        return S_OK;
    return S_FALSE;
}

HRESULT AccessibleBase::get_accKeyboardShortcut(VARIANT vChild, __deref_opt_out BSTR* shortcut)
{
    if (!shortcut)
        return E_POINTER;

    *shortcut = nullptr;

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    String accessKey = childObj->accessKey();
    if (accessKey.isNull())
        return S_FALSE;

    static String accessKeyModifiers;
    if (accessKeyModifiers.isNull()) {
        StringBuilder accessKeyModifiersBuilder;
        auto modifiers = EventHandler::accessKeyModifiers();
        // Follow the same order as Mozilla MSAA implementation:
        // Ctrl+Alt+Shift+Meta+key. MSDN states that keyboard shortcut strings
        // should not be localized and defines the separator as "+".
        if (modifiers.contains(PlatformEvent::Modifier::ControlKey))
            accessKeyModifiersBuilder.appendLiteral("Ctrl+");
        if (modifiers.contains(PlatformEvent::Modifier::AltKey))
            accessKeyModifiersBuilder.appendLiteral("Alt+");
        if (modifiers.contains(PlatformEvent::Modifier::ShiftKey))
            accessKeyModifiersBuilder.appendLiteral("Shift+");
        if (modifiers.contains(PlatformEvent::Modifier::MetaKey))
            accessKeyModifiersBuilder.appendLiteral("Win+");
        accessKeyModifiers = accessKeyModifiersBuilder.toString();
    }
    *shortcut = BString(String(accessKeyModifiers + accessKey)).release();
    return S_OK;
}

HRESULT AccessibleBase::accSelect(long selectionFlags, VARIANT vChild)
{
    // According to MSDN, these combinations are invalid.
    if (((selectionFlags & (SELFLAG_ADDSELECTION | SELFLAG_REMOVESELECTION)) == (SELFLAG_ADDSELECTION | SELFLAG_REMOVESELECTION))
        || ((selectionFlags & (SELFLAG_ADDSELECTION | SELFLAG_TAKESELECTION)) == (SELFLAG_ADDSELECTION | SELFLAG_TAKESELECTION))
        || ((selectionFlags & (SELFLAG_REMOVESELECTION | SELFLAG_TAKESELECTION)) == (SELFLAG_REMOVESELECTION | SELFLAG_TAKESELECTION))
        || ((selectionFlags & (SELFLAG_EXTENDSELECTION | SELFLAG_TAKESELECTION)) == (SELFLAG_EXTENDSELECTION | SELFLAG_TAKESELECTION)))
        return E_INVALIDARG;

    AccessibilityObject* childObject;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObject);

    if (FAILED(hr))
        return hr;

    if (selectionFlags & SELFLAG_TAKEFOCUS)
        childObject->setFocused(true);

    AccessibilityObject* parentObject = childObject->parentObject();
    if (!parentObject)
        return E_INVALIDARG;

    if (selectionFlags & SELFLAG_TAKESELECTION) {
        if (is<AccessibilityListBox>(*parentObject)) {
            Vector<RefPtr<AccessibilityObject> > selectedChildren(1);
            selectedChildren[0] = childObject;
            downcast<AccessibilityListBox>(*parentObject).setSelectedChildren(selectedChildren);
        } else { // any element may be selectable by virtue of it having the aria-selected property
            ASSERT(!parentObject->isMultiSelectable());
            childObject->setSelected(true);
        }
    }

    // MSDN says that ADD, REMOVE, and EXTENDSELECTION with no other flags are invalid for
    // single-select.
    const long allSELFLAGs = SELFLAG_TAKEFOCUS | SELFLAG_TAKESELECTION | SELFLAG_EXTENDSELECTION | SELFLAG_ADDSELECTION | SELFLAG_REMOVESELECTION;
    if (!parentObject->isMultiSelectable()
        && (((selectionFlags & allSELFLAGs) == SELFLAG_ADDSELECTION)
        || ((selectionFlags & allSELFLAGs) == SELFLAG_REMOVESELECTION)
        || ((selectionFlags & allSELFLAGs) == SELFLAG_EXTENDSELECTION)))
        return E_INVALIDARG;

    if (selectionFlags & SELFLAG_ADDSELECTION)
        childObject->setSelected(true);

    if (selectionFlags & SELFLAG_REMOVESELECTION)
        childObject->setSelected(false);

    // FIXME: Should implement SELFLAG_EXTENDSELECTION. For now, we just return
    // S_OK, matching Firefox.

    return S_OK;
}

HRESULT AccessibleBase::get_accSelection(_Out_ VARIANT*)
{
    return E_NOTIMPL;
}

HRESULT AccessibleBase::get_accFocus(_Out_ VARIANT* pvFocusedChild)
{
    if (!pvFocusedChild)
        return E_POINTER;

    ::VariantInit(pvFocusedChild);

    if (!m_object)
        return E_FAIL;

    auto focusedObject = downcast<AccessibilityObject>(m_object->focusedUIElement());
    if (!focusedObject)
        return S_FALSE;

    if (focusedObject == m_object) {
        V_VT(pvFocusedChild) = VT_I4;
        V_I4(pvFocusedChild) = CHILDID_SELF;
        return S_OK;
    }

    V_VT(pvFocusedChild) = VT_DISPATCH;
    V_DISPATCH(pvFocusedChild) = wrapper(focusedObject);
    V_DISPATCH(pvFocusedChild)->AddRef();
    return S_OK;
}

HRESULT AccessibleBase::get_accDefaultAction(VARIANT vChild, __deref_opt_out BSTR* action)
{
    if (!action)
        return E_POINTER;

    *action = nullptr;

    AccessibilityObject* childObj;
    HRESULT hr = getAccessibilityObjectForChild(vChild, childObj);

    if (FAILED(hr))
        return hr;

    if (*action = BString(childObj->actionVerb()).release())
        return S_OK;
    return S_FALSE;
}

HRESULT AccessibleBase::accLocation(_Out_ long* left, _Out_ long* top, _Out_ long* width, _Out_ long* height, VARIANT vChild)
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

    IntRect screenRect(childObj->documentFrameView()->contentsToScreen(snappedIntRect(childObj->elementRect())));
    *left = screenRect.x();
    *top = screenRect.y();
    *width = screenRect.width();
    *height = screenRect.height();
    return S_OK;
}

HRESULT AccessibleBase::accNavigate(long direction, VARIANT vFromChild, _Out_ VARIANT* pvNavigatedTo)
{
    if (!pvNavigatedTo)
        return E_POINTER;

    ::VariantInit(pvNavigatedTo);

    AccessibilityObject* childObj = nullptr;

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

HRESULT AccessibleBase::accHitTest(long x, long y, _Out_ VARIANT* pvChildAtPoint)
{
    if (!pvChildAtPoint)
        return E_POINTER;

    ::VariantInit(pvChildAtPoint);

    if (!m_object || !m_object->documentFrameView())
        return E_FAIL;

    IntPoint point = m_object->documentFrameView()->screenToContents(IntPoint(x, y));
    auto childObject = downcast<AccessibilityObject>(m_object->accessibilityHitTest(point));

    if (!childObject) {
        // If we did not hit any child objects, test whether the point hit us, and
        // report that.
        if (!m_object->boundingBoxRect().contains(point))
            return S_FALSE;
        childObject = m_object;
    }

    if (childObject == m_object) {
        V_VT(pvChildAtPoint) = VT_I4;
        V_I4(pvChildAtPoint) = CHILDID_SELF;
    } else {
        V_VT(pvChildAtPoint) = VT_DISPATCH;
        V_DISPATCH(pvChildAtPoint) = static_cast<IDispatch*>(wrapper(childObject));
        V_DISPATCH(pvChildAtPoint)->AddRef();
    }
    return S_OK;
}

HRESULT AccessibleBase::accDoDefaultAction(VARIANT vChild)
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

static long MSAARole(AccessibilityRole role)
{
    switch (role) {
    case AccessibilityRole::Button:
        return ROLE_SYSTEM_PUSHBUTTON;
    case AccessibilityRole::RadioButton:
        return ROLE_SYSTEM_RADIOBUTTON;
    case AccessibilityRole::CheckBox:
    case AccessibilityRole::ToggleButton:
    case AccessibilityRole::Switch:
        return ROLE_SYSTEM_CHECKBUTTON;
    case AccessibilityRole::Slider:
        return ROLE_SYSTEM_SLIDER;
    case AccessibilityRole::TabGroup:
    case AccessibilityRole::TabList:
        return ROLE_SYSTEM_PAGETABLIST;
    case AccessibilityRole::TextField:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::EditableText:
        return ROLE_SYSTEM_TEXT;
    case AccessibilityRole::Heading:
    case AccessibilityRole::ListMarker:
    case AccessibilityRole::StaticText:
    case AccessibilityRole::Label:
        return ROLE_SYSTEM_STATICTEXT;
    case AccessibilityRole::Outline:
        return ROLE_SYSTEM_OUTLINE;
    case AccessibilityRole::Column:
        return ROLE_SYSTEM_COLUMN;
    case AccessibilityRole::Row:
        return ROLE_SYSTEM_ROW;
    case AccessibilityRole::ApplicationGroup:
    case AccessibilityRole::Group:
    case AccessibilityRole::RadioGroup:
        return ROLE_SYSTEM_GROUPING;
    case AccessibilityRole::DescriptionList:
    case AccessibilityRole::Directory:
    case AccessibilityRole::List:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::MenuListPopup:
        return ROLE_SYSTEM_LIST;
    case AccessibilityRole::Grid:
    case AccessibilityRole::Table:
        return ROLE_SYSTEM_TABLE;
    case AccessibilityRole::ImageMapLink:
    case AccessibilityRole::Link:
    case AccessibilityRole::WebCoreLink:
        return ROLE_SYSTEM_LINK;
    case AccessibilityRole::Canvas:
    case AccessibilityRole::ImageMap:
    case AccessibilityRole::Image:
        return ROLE_SYSTEM_GRAPHIC;
    case AccessibilityRole::ListItem:
        return ROLE_SYSTEM_LISTITEM;
    case AccessibilityRole::ListBoxOption:
    case AccessibilityRole::MenuListOption:
        return ROLE_SYSTEM_STATICTEXT;
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::PopUpButton:
        return ROLE_SYSTEM_COMBOBOX;
    case AccessibilityRole::Div:
    case AccessibilityRole::Footer:
    case AccessibilityRole::Form:
    case AccessibilityRole::Paragraph:
        return ROLE_SYSTEM_GROUPING;
    case AccessibilityRole::HorizontalRule:
    case AccessibilityRole::Splitter:
        return ROLE_SYSTEM_SEPARATOR;
    case AccessibilityRole::ApplicationAlert:
    case AccessibilityRole::ApplicationAlertDialog:
        return ROLE_SYSTEM_ALERT;
    case AccessibilityRole::DisclosureTriangle:
        return ROLE_SYSTEM_BUTTONDROPDOWN;
    case AccessibilityRole::Incrementor:
    case AccessibilityRole::SpinButton:
        return ROLE_SYSTEM_SPINBUTTON;
    case AccessibilityRole::SpinButtonPart:
        return ROLE_SYSTEM_PUSHBUTTON;
    case AccessibilityRole::Toolbar:
        return ROLE_SYSTEM_TOOLBAR;
    case AccessibilityRole::UserInterfaceTooltip:
        return ROLE_SYSTEM_TOOLTIP;
    case AccessibilityRole::Tree:
    case AccessibilityRole::TreeGrid:
        return ROLE_SYSTEM_OUTLINE;
    case AccessibilityRole::TreeItem:
        return ROLE_SYSTEM_OUTLINEITEM;
    case AccessibilityRole::TabPanel:
        return ROLE_SYSTEM_GROUPING;
    case AccessibilityRole::Tab:
        return ROLE_SYSTEM_PAGETAB;
    case AccessibilityRole::Application:
        return ROLE_SYSTEM_APPLICATION;
    case AccessibilityRole::ApplicationDialog:
        return ROLE_SYSTEM_DIALOG;
    case AccessibilityRole::ApplicationLog:
    case AccessibilityRole::ApplicationMarquee:
        return ROLE_SYSTEM_GROUPING;
    case AccessibilityRole::ApplicationStatus:
        return ROLE_SYSTEM_STATUSBAR;
    case AccessibilityRole::ApplicationTimer:
        return ROLE_SYSTEM_CLOCK;
    case AccessibilityRole::Cell:
        return ROLE_SYSTEM_CELL;
    case AccessibilityRole::ColumnHeader:
        return ROLE_SYSTEM_COLUMNHEADER;
    case AccessibilityRole::Definition:
    case AccessibilityRole::DescriptionListDetail:
    case AccessibilityRole::DescriptionListTerm:
    case AccessibilityRole::Document:
    case AccessibilityRole::DocumentArticle:
    case AccessibilityRole::DocumentNote:
        return ROLE_SYSTEM_GROUPING;
    case AccessibilityRole::DocumentMath:
    case AccessibilityRole::MathElement:
        return ROLE_SYSTEM_EQUATION;
    case AccessibilityRole::HelpTag:
        return ROLE_SYSTEM_HELPBALLOON;
    case AccessibilityRole::WebApplication:
    case AccessibilityRole::LandmarkBanner:
    case AccessibilityRole::LandmarkComplementary:
    case AccessibilityRole::LandmarkContentInfo:
    case AccessibilityRole::LandmarkMain:
    case AccessibilityRole::LandmarkNavigation:
    case AccessibilityRole::LandmarkRegion:
    case AccessibilityRole::LandmarkSearch:
    case AccessibilityRole::Legend:
        return ROLE_SYSTEM_GROUPING;
    case AccessibilityRole::Menu:
        return ROLE_SYSTEM_MENUPOPUP;
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuButton:
        return ROLE_SYSTEM_MENUITEM;
    case AccessibilityRole::MenuBar:
        return ROLE_SYSTEM_MENUBAR;
    case AccessibilityRole::ProgressIndicator:
        return ROLE_SYSTEM_PROGRESSBAR;
    case AccessibilityRole::RowHeader:
        return ROLE_SYSTEM_ROWHEADER;
    case AccessibilityRole::ScrollBar:
        return ROLE_SYSTEM_SCROLLBAR;
    case AccessibilityRole::SVGRoot:
        return ROLE_SYSTEM_GROUPING;
    case AccessibilityRole::TableHeaderContainer:
        return ROLE_SYSTEM_GROUPING;
    case AccessibilityRole::Window:
        return ROLE_SYSTEM_WINDOW;
    default:
        // This is the default role for MSAA.
        return ROLE_SYSTEM_CLIENT;
    }
}

long AccessibleBase::role() const
{
    return MSAARole(m_object->roleValueForMSAA());
}

HRESULT AccessibleBase::getAccessibilityObjectForChild(VARIANT vChild, AccessibilityObject*& childObj) const
{
    childObj = 0;

    if (!m_object)
        return E_FAIL;

    m_object->updateBackingStore();

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

AccessibleBase* AccessibleBase::wrapper(AccessibilityObject* obj) const
{
    AccessibleBase* result = static_cast<AccessibleBase*>(obj->wrapper());
    if (!result)
        result = createInstance(obj, m_window);
    return result;
}

HRESULT AccessibleBase::isSameObject(_In_opt_ IAccessibleComparable* other, _Out_ BOOL* result)
{
    if (!result || !other)
        return E_POINTER;

    COMPtr<AccessibleBase> otherAccessibleBase(Query, other);
    *result = (otherAccessibleBase == this || otherAccessibleBase->m_object == m_object);
    return S_OK;
}
