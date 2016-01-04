/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
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

    virtual void show(const IntRect&, FrameView*, int index);
    virtual void hide();
    virtual void updateFromElement();
    virtual void disconnectClient();

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
    virtual int scrollSize(ScrollbarOrientation) const override;
    virtual int scrollOffset(ScrollbarOrientation) const override;
    virtual void setScrollOffset(const IntPoint&) override;
    virtual void invalidateScrollbarRect(Scrollbar*, const IntRect&) override;
    virtual void invalidateScrollCornerRect(const IntRect&) override { }
    virtual bool isActive() const override { return true; }
    ScrollableArea* enclosingScrollableArea() const override { return 0; }
    virtual bool isScrollableOrRubberbandable() override { return true; }
    virtual bool hasScrollableOrRubberbandableAncestor() override { return true; }
    virtual bool isScrollCornerVisible() const override { return false; }
    virtual IntRect scrollCornerRect() const override { return IntRect(); }
    virtual Scrollbar* verticalScrollbar() const override { return m_scrollbar.get(); }
    virtual IntSize visibleSize() const override;
    virtual IntSize contentsSize() const override;
    virtual IntRect scrollableAreaBoundingBox(bool* = nullptr) const override;
    virtual bool updatesScrollLayerPositionOnMainThread() const override { return true; }
    virtual bool forceUpdateScrollbarsOnMainThreadForPerformanceTesting() const override { return false; }

    // NOTE: This should only be called by the overriden setScrollOffset from ScrollableArea.
    void scrollTo(int offset);

    void calculatePositionAndSize(const IntRect&, FrameView*);
    void invalidateItem(int index);

    bool onGetObject(WPARAM wParam, LPARAM lParam, LRESULT& lResult);

    static LRESULT CALLBACK PopupMenuWndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static void registerClass();

    PopupMenuClient* m_popupClient;
    RefPtr<Scrollbar> m_scrollbar;
    HWND m_popup;
    GDIObject<HDC> m_DC;
    GDIObject<HBITMAP> m_bmp;
    bool m_wasClicked;
    IntRect m_windowRect;
    int m_itemHeight;
    int m_scrollOffset;
    int m_wheelDelta;
    int m_focusedIndex;
    int m_hoveredIndex;
    bool m_scrollbarCapturingMouse;
    bool m_showPopup;
    COMPtr<IAccessible> m_accessiblePopupMenu;

    friend class AccessiblePopupMenu;
};

class AccessiblePopupMenu : public IAccessible {
public:
    AccessiblePopupMenu(const PopupMenuWin&);
    ~AccessiblePopupMenu();

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, __RPC__deref_out void __RPC_FAR *__RPC_FAR *ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef();
    virtual ULONG STDMETHODCALLTYPE Release();

    // IDispatch - Not to be implemented.
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count);
    virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo** ppTInfo);
    virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*);
    virtual HRESULT STDMETHODCALLTYPE Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*);

    // IAccessible
    virtual HRESULT STDMETHODCALLTYPE get_accParent(IDispatch**);
    virtual HRESULT STDMETHODCALLTYPE get_accChildCount(long*);
    virtual HRESULT STDMETHODCALLTYPE get_accChild(VARIANT vChild, IDispatch** ppChild);
    virtual HRESULT STDMETHODCALLTYPE get_accName(VARIANT vChild, BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_accValue(VARIANT vChild, BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT, BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_accRole(VARIANT vChild, VARIANT* pvRole);
    virtual HRESULT STDMETHODCALLTYPE get_accState(VARIANT vChild, VARIANT* pvState);
    virtual HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT vChild, BSTR* helpText);
    virtual HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(VARIANT vChild, BSTR*);
    virtual HRESULT STDMETHODCALLTYPE get_accFocus(VARIANT* pvFocusedChild);
    virtual HRESULT STDMETHODCALLTYPE get_accSelection(VARIANT* pvSelectedChild);
    virtual HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT vChild, BSTR* actionDescription);
    virtual HRESULT STDMETHODCALLTYPE accSelect(long selectionFlags, VARIANT vChild);
    virtual HRESULT STDMETHODCALLTYPE accLocation(long* left, long* top, long* width, long* height, VARIANT vChild);
    virtual HRESULT STDMETHODCALLTYPE accNavigate(long direction, VARIANT vFromChild, VARIANT* pvNavigatedTo);
    virtual HRESULT STDMETHODCALLTYPE accHitTest(long x, long y, VARIANT* pvChildAtPoint);
    virtual HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT vChild);
    virtual HRESULT STDMETHODCALLTYPE put_accName(VARIANT, BSTR);
    virtual HRESULT STDMETHODCALLTYPE put_accValue(VARIANT, BSTR);
    virtual HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR* helpFile, VARIANT, long* topicID);

private:
    int m_refCount;
    const PopupMenuWin& m_popupMenu;
};

} // namespace WebCore

#endif // PopupMenuWin_h
