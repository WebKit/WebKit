/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NetscapePlugin.h"

#include "PluginController.h"
#include "WebEvent.h"
#include <WebCore/DefWndProcWindowClass.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/LocalWindowsContext.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/WebCoreInstanceHandle.h>

using namespace WebCore;

namespace WebKit {

static NetscapePlugin* currentPlugin;

class CurrentPluginSetter {
    WTF_MAKE_NONCOPYABLE(CurrentPluginSetter);
public:
    explicit CurrentPluginSetter(NetscapePlugin* plugin)
        : m_plugin(plugin)
        , m_formerCurrentPlugin(currentPlugin)
    {
        currentPlugin = m_plugin;
    }

    ~CurrentPluginSetter()
    {
        ASSERT(currentPlugin == m_plugin);
        currentPlugin = m_formerCurrentPlugin;
    }

private:
    NetscapePlugin* m_plugin;
    NetscapePlugin* m_formerCurrentPlugin;
};

static LPCWSTR windowClassName = L"org.webkit.NetscapePluginWindow";

static void registerPluginView()
{
    static bool didRegister;
    if (didRegister)
        return;
    didRegister = true;

    WNDCLASSW windowClass = {0};
    windowClass.style = CS_DBLCLKS;
    windowClass.lpfnWndProc = ::DefWindowProcW;
    windowClass.hInstance = instanceHandle();
    windowClass.hCursor = ::LoadCursorW(0, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = windowClassName;

    ::RegisterClassW(&windowClass);
}

HWND NetscapePlugin::containingWindow() const
{
    return m_pluginController->nativeParentWindow();
}

bool NetscapePlugin::platformPostInitialize()
{
    if (!m_isWindowed) {
        m_window = 0;

        // Windowless plugins need a little help showing context menus since our containingWindow()
        // is in a different process. See <http://webkit.org/b/51063>.
        m_pluginModule->module()->installIATHook("user32.dll", "TrackPopupMenu", hookedTrackPopupMenu);
        m_contextMenuOwnerWindow = ::CreateWindowExW(0, defWndProcWindowClassName(), 0, WS_CHILD, 0, 0, 0, 0, containingWindow(), 0, instanceHandle(), 0);

        return true;
    }

    registerPluginView();

    // Start out with the window hidden. The UI process will take care of showing it once it's correctly positioned the window.
    m_window = ::CreateWindowExW(0, windowClassName, 0, WS_CHILD, 0, 0, 0, 0, containingWindow(), 0, instanceHandle(), 0);
    if (!m_window)
        return false;

    // FIXME: Do we need to pass our window to setPlatformWidget?
    // FIXME: WebCore::PluginView sets the window proc to DefWindowProcA here for Shockwave Director.

    m_npWindow.type = NPWindowTypeWindow;
    m_npWindow.window = m_window;

    return true;
}

void NetscapePlugin::platformDestroy()
{
    if (!m_isWindowed) {
        ASSERT(m_contextMenuOwnerWindow);
        ::DestroyWindow(m_contextMenuOwnerWindow);
        ASSERT(!m_window);
        return;
    }

    if (!::IsWindow(m_window))
        return;
    ::DestroyWindow(m_window);
}

bool NetscapePlugin::platformInvalidate(const IntRect& invalidRect)
{
    if (!m_isWindowed)
        return false;

    RECT rect = invalidRect;
    ::InvalidateRect(m_window, &rect, FALSE);
    return true;
}

void NetscapePlugin::platformGeometryDidChange()
{
    if (!m_isWindowed)
        return;

    IntRect clipRectInPluginWindowCoordinates = m_clipRect;
    clipRectInPluginWindowCoordinates.move(-m_frameRect.x(), -m_frameRect.y());

    // We only update the size here and let the UI process update our position and clip rect so
    // that we can keep our position in sync when scrolling, etc. See <http://webkit.org/b/60210>.
    ::SetWindowPos(m_window, 0, 0, 0, m_frameRect.width(), m_frameRect.height(), SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);

    m_pluginController->scheduleWindowedPluginGeometryUpdate(m_window, m_frameRect, clipRectInPluginWindowCoordinates);
}

void NetscapePlugin::platformPaint(GraphicsContext* context, const IntRect& dirtyRect, bool)
{
    CurrentPluginSetter setCurrentPlugin(this);

    // FIXME: Call SetWindow here if we haven't called it yet (see r59904).

    if (m_isWindowed) {
        // FIXME: Paint windowed plugins into context if context->shouldIncludeChildWindows() is true.
        return;
    }

    m_pluginController->willSendEventToPlugin();
    
    LocalWindowsContext windowsContext(context, dirtyRect, m_isTransparent);

    m_npWindow.type = NPWindowTypeDrawable;
    m_npWindow.window = windowsContext.hdc();

    WINDOWPOS windowpos = { 0, 0, 0, 0, 0, 0, 0 };

    windowpos.x = m_frameRect.x();
    windowpos.y = m_frameRect.y();
    windowpos.cx = m_frameRect.width();
    windowpos.cy = m_frameRect.height();

    NPEvent npEvent;
    npEvent.event = WM_WINDOWPOSCHANGED;
    npEvent.wParam = 0;
    npEvent.lParam = reinterpret_cast<uintptr_t>(&windowpos);

    NPP_HandleEvent(&npEvent);

    callSetWindow();

    RECT dirtyWinRect = dirtyRect;

    npEvent.event = WM_PAINT;
    npEvent.wParam = reinterpret_cast<uintptr_t>(windowsContext.hdc());
    npEvent.lParam = reinterpret_cast<uintptr_t>(&dirtyWinRect);

    NPP_HandleEvent(&npEvent);
}

NPEvent toNP(const WebMouseEvent& event)
{
    NPEvent npEvent;

    npEvent.wParam = 0;
    if (event.controlKey())
        npEvent.wParam |= MK_CONTROL;
    if (event.shiftKey())
        npEvent.wParam |= MK_SHIFT;

    npEvent.lParam = MAKELPARAM(event.position().x(), event.position().y());

    switch (event.type()) {
    case WebEvent::MouseMove:
        npEvent.event = WM_MOUSEMOVE;
        switch (event.button()) {
        case WebMouseEvent::LeftButton:
            npEvent.wParam |= MK_LBUTTON;
            break;
        case WebMouseEvent::MiddleButton:
            npEvent.wParam |= MK_MBUTTON;
            break;
        case WebMouseEvent::RightButton:
            npEvent.wParam |= MK_RBUTTON;
            break;
        case WebMouseEvent::NoButton:
            break;
        }
        break;
    case WebEvent::MouseDown:
        switch (event.button()) {
        case WebMouseEvent::LeftButton:
            npEvent.event = WM_LBUTTONDOWN;
            break;
        case WebMouseEvent::MiddleButton:
            npEvent.event = WM_MBUTTONDOWN;
            break;
        case WebMouseEvent::RightButton:
            npEvent.event = WM_RBUTTONDOWN;
            break;
        case WebMouseEvent::NoButton:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    case WebEvent::MouseUp:
        switch (event.button()) {
        case WebMouseEvent::LeftButton:
            npEvent.event = WM_LBUTTONUP;
            break;
        case WebMouseEvent::MiddleButton:
            npEvent.event = WM_MBUTTONUP;
            break;
        case WebMouseEvent::RightButton:
            npEvent.event = WM_RBUTTONUP;
            break;
        case WebMouseEvent::NoButton:
            ASSERT_NOT_REACHED();
            break;
        }
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return npEvent;
}

bool NetscapePlugin::platformHandleMouseEvent(const WebMouseEvent& event)
{
    CurrentPluginSetter setCurrentPlugin(this);

    if (m_isWindowed)
        return false;

    m_pluginController->willSendEventToPlugin();

    NPEvent npEvent = toNP(event);
    NPP_HandleEvent(&npEvent);
    return true;
}

bool NetscapePlugin::platformHandleWheelEvent(const WebWheelEvent&)
{
    CurrentPluginSetter setCurrentPlugin(this);

    notImplemented();
    return false;
}

void NetscapePlugin::platformSetFocus(bool)
{
    CurrentPluginSetter setCurrentPlugin(this);

    notImplemented();
}

bool NetscapePlugin::platformHandleMouseEnterEvent(const WebMouseEvent& event)
{
    CurrentPluginSetter setCurrentPlugin(this);

    if (m_isWindowed)
        return false;

    m_pluginController->willSendEventToPlugin();

    NPEvent npEvent = toNP(event);
    NPP_HandleEvent(&npEvent);
    return true;
}

bool NetscapePlugin::platformHandleMouseLeaveEvent(const WebMouseEvent& event)
{
    CurrentPluginSetter setCurrentPlugin(this);

    if (m_isWindowed)
        return false;

    m_pluginController->willSendEventToPlugin();

    NPEvent npEvent = toNP(event);
    NPP_HandleEvent(&npEvent);
    return true;
}

bool NetscapePlugin::platformHandleKeyboardEvent(const WebKeyboardEvent&)
{
    CurrentPluginSetter setCurrentPlugin(this);

    notImplemented();
    return false;
}

BOOL NetscapePlugin::hookedTrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, const RECT* prcRect)
{
    // ::TrackPopupMenu fails when it is passed a window that is owned by another thread. If this
    // happens, we substitute a dummy window that is owned by this thread.

    if (::GetWindowThreadProcessId(hWnd, 0) == ::GetCurrentThreadId())
        return ::TrackPopupMenu(hMenu, uFlags, x, y, nReserved, hWnd, prcRect);

    HWND originalFocusWindow = 0;

    ASSERT(currentPlugin);
    if (currentPlugin) {
        ASSERT(!currentPlugin->m_isWindowed);
        ASSERT(currentPlugin->m_contextMenuOwnerWindow);
        ASSERT(::GetWindowThreadProcessId(currentPlugin->m_contextMenuOwnerWindow, 0) == ::GetCurrentThreadId());
        hWnd = currentPlugin->m_contextMenuOwnerWindow;

        // If we don't focus the dummy window, the user will be able to scroll the page while the
        // context menu is up, e.g.
        originalFocusWindow = ::SetFocus(hWnd);
    }

    BOOL result = ::TrackPopupMenu(hMenu, uFlags, x, y, nReserved, hWnd, prcRect);

    if (originalFocusWindow)
        ::SetFocus(originalFocusWindow);

    return result;
}

} // namespace WebKit
