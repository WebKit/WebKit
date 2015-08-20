/*
 * Copyright (C) 2008-2010, 2013, 2015 Apple Inc. All Rights Reserved.
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

#ifndef AccessibleBase_h
#define AccessibleBase_h

#include "AccessibleEditableText.h"
#include "AccessibleText.h"
#include "AccessibleText2.h"
#include "WebKit.h"
#include <WebCore/AccessibilityObject.h>
#include <WebCore/AccessibilityObjectWrapperWin.h>

class DECLSPEC_UUID("3dbd565b-db22-4d88-8e0e-778bde54524a") AccessibleBase : public IAccessibleComparable, public IServiceProvider, public WebCore::AccessibilityObjectWrapper {
public:
    static AccessibleBase* createInstance(WebCore::AccessibilityObject*, HWND);

    // IServiceProvider
    virtual HRESULT STDMETHODCALLTYPE QueryService(_In_ REFGUID guidService, _In_ REFIID riid, _COM_Outptr_ void **ppv);

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return ++m_refCount; }
    virtual ULONG STDMETHODCALLTYPE Release();

    // IAccessible2_2
    virtual HRESULT STDMETHODCALLTYPE get_attribute(_In_ BSTR name, _Out_ VARIANT* attribute);
    virtual HRESULT STDMETHODCALLTYPE get_accessibleWithCaret(_COM_Outptr_opt_ IUnknown** accessible, _Out_ long* caretOffset);
    virtual HRESULT STDMETHODCALLTYPE get_relationTargetsOfType(_In_ BSTR type, long maxTargets, __deref_out_ecount_full_opt(*nTargets) IUnknown*** targets, _Out_ long* nTargets);

    // IAccessible2
    virtual HRESULT STDMETHODCALLTYPE get_nRelations(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE get_relation(long relationIndex, _COM_Outptr_opt_ IAccessibleRelation**);
    virtual HRESULT STDMETHODCALLTYPE get_relations(long maxRelations, __out_ecount_part(maxRelations, *nRelations) IAccessibleRelation**, _Out_ long* nRelations);
    virtual HRESULT STDMETHODCALLTYPE role(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE scrollTo(IA2ScrollType);
    virtual HRESULT STDMETHODCALLTYPE scrollToPoint(IA2CoordinateType, long x, long y);
    virtual HRESULT STDMETHODCALLTYPE get_groupPosition(_Out_ long* groupLevel, _Out_ long* similarItemsInGroup, _Out_ long* positionInGroup);
    virtual HRESULT STDMETHODCALLTYPE get_states(_Out_ AccessibleStates*);
    virtual HRESULT STDMETHODCALLTYPE get_extendedRole(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_localizedExtendedRole(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_nExtendedStates(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE get_extendedStates(long maxExtendedStates, __deref_out_ecount_part_opt(maxExtendedStates, *nExtendedStates) BSTR** extendedStates, _Out_ long* nExtendedStates);
    virtual HRESULT STDMETHODCALLTYPE get_localizedExtendedStates(long maxLocalizedExtendedStates, __deref_out_ecount_part_opt(maxLocalizedExtendedStates, *nLocalizedExtendedStates) BSTR** localizedExtendedStates, _Out_ long* nLocalizedExtendedStates);
    virtual HRESULT STDMETHODCALLTYPE get_uniqueID(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE get_windowHandle(_Out_ HWND*);
    virtual HRESULT STDMETHODCALLTYPE get_indexInParent(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE get_locale(_Out_ IA2Locale*);
    virtual HRESULT STDMETHODCALLTYPE get_attributes(__deref_opt_out BSTR*);

    // IAccessible
    virtual HRESULT STDMETHODCALLTYPE get_accParent(_COM_Outptr_opt_ IDispatch**);
    virtual HRESULT STDMETHODCALLTYPE get_accChildCount(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE get_accChild(VARIANT vChild, _COM_Outptr_opt_ IDispatch** ppChild);
    virtual HRESULT STDMETHODCALLTYPE get_accName(VARIANT vChild, __deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_accValue(VARIANT vChild, __deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT, __deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_accRole(VARIANT vChild, _Out_ VARIANT* pvRole);
    virtual HRESULT STDMETHODCALLTYPE get_accState(VARIANT vChild, _Out_ VARIANT* pvState);
    virtual HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT vChild, __deref_opt_out BSTR* helpText);
    virtual HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(VARIANT vChild, __deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_accFocus(_Out_ VARIANT* pvFocusedChild);
    virtual HRESULT STDMETHODCALLTYPE get_accSelection(_Out_ VARIANT* pvSelectedChild);
    virtual HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT vChild, __deref_opt_out BSTR* actionDescription);
    virtual HRESULT STDMETHODCALLTYPE accSelect(long selectionFlags, VARIANT vChild);
    virtual HRESULT STDMETHODCALLTYPE accLocation(_Out_ long* left, _Out_ long* top, _Out_ long* width, _Out_ long* height, VARIANT vChild);
    virtual HRESULT STDMETHODCALLTYPE accNavigate(long direction, VARIANT vFromChild, _Out_ VARIANT* pvNavigatedTo);
    virtual HRESULT STDMETHODCALLTYPE accHitTest(long x, long y, _Out_ VARIANT* pvChildAtPoint);
    virtual HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT vChild);

    // IAccessible - Not to be implemented.
    virtual HRESULT STDMETHODCALLTYPE put_accName(VARIANT, _In_ BSTR) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE put_accValue(VARIANT, _In_ BSTR) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR* helpFile, VARIANT, _Out_ long* topicID)
    {
        if (!helpFile || !topicID)
            return E_POINTER;

        *helpFile = nullptr;
        *topicID = 0;
        return E_NOTIMPL;
    }

    // IDispatch - Not to be implemented.
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(_Out_ UINT* count)
    {
        if (!count)
            return E_POINTER;
        *count = 0;
        return E_NOTIMPL;
    }
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, __deref_out_opt ITypeInfo** ppTInfo)
    {
        if (!ppTInfo)
            return E_POINTER;
        *ppTInfo = nullptr;
        return E_NOTIMPL;
    }
    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(_In_ REFIID, LPOLESTR*, UINT, LCID, DISPID*) { return E_NOTIMPL; }
    virtual HRESULT STDMETHODCALLTYPE Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) { return E_NOTIMPL; }

    // WebCore::AccessibilityObjectWrapper
    virtual void detach()
    {
        ASSERT(m_object);
        m_object = nullptr;
    }

    // IAccessibleComparable
    virtual HRESULT STDMETHODCALLTYPE isSameObject(_In_opt_ IAccessibleComparable* other, _Out_ BOOL* result);

protected:
    AccessibleBase(WebCore::AccessibilityObject*, HWND);
    virtual ~AccessibleBase();

    virtual WTF::String name() const;
    virtual WTF::String value() const;
    virtual long role() const;
    virtual long state() const;

    HRESULT getAccessibilityObjectForChild(VARIANT vChild, WebCore::AccessibilityObject*&) const;

    AccessibleBase* wrapper(WebCore::AccessibilityObject*) const;

    HWND m_window;
    int m_refCount { 0 };

private:
    AccessibleBase() { }
};

#endif // AccessibleBase_h

