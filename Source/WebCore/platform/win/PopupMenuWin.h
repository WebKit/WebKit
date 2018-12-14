/*
 * Copyright (C) 2011, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef PopupMenuWin_h
#define PopupMenuWin_h

#include "COMPtr.h"
#include "IntRect.h"
#include "PopupMenu.h"
#include "PopupMenuClient.h"
#include "ScrollableArea.h"
#include "Scrollbar.h"
#include <OleAcc.h>
#include <wtf/RefPtr.h>
#include <wtf/win/GDIObject.h>

namespace WebCore {

class FrameView;
class Scrollbar;
class AccessiblePopupMenu;

class PopupMenuWin : public PopupMenu, private ScrollableArea {
public:
    PopupMenuWin(PopupMenuClient*);
    ~PopupMenuWin();

    void show(const IntRect&, FrameView*, int index) override;
    void hide() override;
    void updateFromElement() override;
    void disconnectClient() override;

    static LPCWSTR popupClassName();

private:
    PopupMenuClient* client() const { return m_popupClient; }

    Scrollbar* scrollbar() const { return m_scrollbar.get(); }

    bool up(unsigned lines = 1);
    bool down(unsigned lines = 1);

    int itemHeight() const { return m_itemHeight; }
    const IntRect& windowRect() const { return m_windowRect; }
    IntRect clientRect() const;

    int visibleItems() const;

    int listIndexAtPoint(const IntPoint&) const;

    bool setFocusedIndex(int index, bool hotTracking = false);
    int focusedIndex() const;
    void focusFirst();
    void focusLast();

    void paint(const IntRect& damageRect, HDC = 0);

    HWND popupHandle() const { return m_popup; }

    void setWasClicked(bool b = true) { m_wasClicked = b; }
    bool wasClicked() const { return m_wasClicked; }

    int scrollOffset() const { return m_scrollOffset; }

    bool scrollToRevealSelection();

    void incrementWheelDelta(int delta);
    void reduceWheelDelta(int delta);
    int wheelDelta() const { return m_wheelDelta; }

    bool scrollbarCapturingMouse() const { return m_scrollbarCapturingMouse; }
    void setScrollbarCapturingMouse(bool b) { m_scrollbarCapturingMouse = b; }

    // ScrollableArea
    int scrollSize(ScrollbarOrientation) const override;
    int scrollOffset(ScrollbarOrientation) const override;
    void setScrollOffset(const IntPoint&) override;
    void invalidateScrollbarRect(Scrollbar&, const IntRect&) override;
    void invalidateScrollCornerRect(const IntRect&) override { }
    bool isActive() const override { return true; }
    ScrollableArea* enclosingScrollableArea() const override { return 0; }
    bool isScrollableOrRubberbandable() override { return true; }
    bool hasScrollableOrRubberbandableAncestor() override { return true; }
    bool isScrollCornerVisible() const override { return false; }
    IntRect scrollCornerRect() const override { return IntRect(); }
    Scrollbar* verticalScrollbar() const override { return m_scrollbar.get(); }
    IntSize visibleSize() const override;
    IntSize contentsSize() const override;
    IntRect scrollableAreaBoundingBox(bool* = nullptr) const override;
    bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const override { return false; }
    bool shouldPlaceBlockDirectionScrollbarOnLeft() const final { return false; }

    // NOTE: This should only be called by the overriden setScrollOffset from ScrollableArea.
    void scrollTo(int offset);

    void calculatePositionAndSize(const IntRect&, FrameView*);
    void invalidateItem(int index);

    bool onGetObject(WPARAM wParam, LPARAM lParam, LRESULT& lResult);

    static LRESULT CALLBACK PopupMenuWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static void registerClass();

    RefPtr<Scrollbar> m_scrollbar;
    COMPtr<IAccessible> m_accessiblePopupMenu;
    GDIObject<HDC> m_DC;
    GDIObject<HBITMAP> m_bmp;
    PopupMenuClient* m_popupClient;
    HWND m_popup { nullptr };
    IntRect m_windowRect;
    int m_itemHeight { 0 };
    int m_scrollOffset { 0 };
    int m_wheelDelta { 0 };
    int m_focusedIndex { 0 };
    int m_hoveredIndex { 0 };
    bool m_wasClicked { false };
    bool m_scrollbarCapturingMouse { false };
    bool m_showPopup { false };
    float m_scaleFactor { 1 };
    FontCascade m_font;

    friend class AccessiblePopupMenu;
};

class AccessiblePopupMenu final : public IAccessible {
public:
    AccessiblePopupMenu(const PopupMenuWin&);
    ~AccessiblePopupMenu();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID, _COM_Outptr_ void**);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IDispatch - Not to be implemented.
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(_Out_ UINT* count);
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, _COM_Outptr_opt_ ITypeInfo**);
    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(_In_ REFIID, __in_ecount(cNames) LPOLESTR*, UINT cNames, LCID, __out_ecount_full(cNames) DISPID*);
    virtual HRESULT STDMETHODCALLTYPE Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*);

    // IAccessible
    virtual HRESULT STDMETHODCALLTYPE get_accParent(_COM_Outptr_opt_ IDispatch**);
    virtual HRESULT STDMETHODCALLTYPE get_accChildCount(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE get_accChild(VARIANT vChild, _COM_Outptr_opt_ IDispatch** ppChild);
    virtual HRESULT STDMETHODCALLTYPE get_accName(VARIANT vChild, __deref_out_opt BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_accValue(VARIANT vChild, __deref_out_opt BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT, __deref_out_opt BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_accRole(VARIANT vChild, _Out_ VARIANT* pvRole);
    virtual HRESULT STDMETHODCALLTYPE get_accState(VARIANT vChild, _Out_ VARIANT* pvState);
    virtual HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT vChild, __deref_out_opt BSTR* helpText);
    virtual HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(VARIANT vChild, __deref_out_opt BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_accFocus(_Out_ VARIANT* pvFocusedChild);
    virtual HRESULT STDMETHODCALLTYPE get_accSelection(_Out_ VARIANT* pvSelectedChild);
    virtual HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT vChild, __deref_out_opt BSTR* actionDescription);
    virtual HRESULT STDMETHODCALLTYPE accSelect(long selectionFlags, VARIANT vChild);
    virtual HRESULT STDMETHODCALLTYPE accLocation(_Out_ long* left, _Out_ long* top, _Out_ long* width, _Out_ long* height, VARIANT vChild);
    virtual HRESULT STDMETHODCALLTYPE accNavigate(long direction, VARIANT vFromChild, _Out_ VARIANT* pvNavigatedTo);
    virtual HRESULT STDMETHODCALLTYPE accHitTest(long x, long y, _Out_ VARIANT* pvChildAtPoint);
    virtual HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT vChild);
    virtual HRESULT STDMETHODCALLTYPE put_accName(VARIANT, _In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE put_accValue(VARIANT, _In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR* helpFile, VARIANT, _Out_ long* topicID);

private:
    int m_refCount { 0 };
    const PopupMenuWin& m_popupMenu;
};

} // namespace WebCore

#endif // PopupMenuWin_h
