/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora Ltd. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "PluginView.h"

#include "Document.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "EventNames.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FrameTree.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "HTMLNames.h"
#include "HTMLPlugInElement.h"
#include "JSDOMWindow.h"
#include "KeyboardEvent.h"
#include "MIMETypeRegistry.h"
#include "MouseEvent.h"
#include "NotImplemented.h"
#include "Page.h"
#include "FocusController.h"
#include "PlatformMouseEvent.h"
#include "PluginMessageThrottlerWin.h"
#include "PluginPackage.h"
#include "JSDOMBinding.h"
#include "ScriptController.h"
#include "PluginDatabase.h"
#include "PluginDebug.h"
#include "PluginPackage.h"
#include "c_instance.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include "Settings.h"
#include "runtime.h"
#include <kjs/JSLock.h>
#include <kjs/JSValue.h>
#include <wtf/ASCIICType.h>

using KJS::ExecState;
using KJS::JSLock;
using KJS::JSObject;
using KJS::JSValue;
using KJS::UString;

using std::min;

using namespace WTF;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

const LPCWSTR kWebPluginViewdowClassName = L"WebPluginView";
const LPCWSTR kWebPluginViewProperty = L"WebPluginViewProperty";

static const char* MozillaUserAgent = "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-US; rv:1.8.1) Gecko/20061010 Firefox/2.0";

static LRESULT CALLBACK PluginViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

static bool registerPluginView()
{
    static bool haveRegisteredWindowClass = false;
    if (haveRegisteredWindowClass)
        return true;

    haveRegisteredWindowClass = true;

    ASSERT(Page::instanceHandle());

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_DBLCLKS;
    wcex.lpfnWndProc    = DefWindowProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = Page::instanceHandle();
    wcex.hIcon          = 0;
    wcex.hCursor        = LoadCursor(0, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)COLOR_WINDOW;
    wcex.lpszMenuName   = 0;
    wcex.lpszClassName  = kWebPluginViewdowClassName;
    wcex.hIconSm        = 0;

    return !!RegisterClassEx(&wcex);
}

static LRESULT CALLBACK PluginViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    PluginView* pluginView = reinterpret_cast<PluginView*>(GetProp(hWnd, kWebPluginViewProperty));

    return pluginView->wndProc(hWnd, message, wParam, lParam);
}

static bool isWindowsMessageUserGesture(UINT message)
{
    switch (message) {
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
        case WM_KEYUP:
            return true;
        default:
            return false;
    }
}

LRESULT
PluginView::wndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // <rdar://5711136> Sometimes Flash will call SetCapture before creating
    // a full-screen window and will not release it, which causes the
    // full-screen window to never receive mouse events. We set/release capture
    // on mouse down/up before sending the event to the plug-in to prevent that.
    switch (message) {
        case WM_LBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_RBUTTONDOWN:
            ::SetCapture(hWnd);
            break;
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
            ::ReleaseCapture();
            break;
    }

    if (message == m_lastMessage &&
        m_plugin->quirks().contains(PluginQuirkDontCallWndProcForSameMessageRecursively) && 
        m_isCallingPluginWndProc)
        return 1;

    if (message == WM_USER + 1 &&
        m_plugin->quirks().contains(PluginQuirkThrottleWMUserPlusOneMessages)) {
        if (!m_messageThrottler)
            m_messageThrottler.set(new PluginMessageThrottlerWin(this));

        m_messageThrottler->appendMessage(hWnd, message, wParam, lParam);
        return 0;
    }

    m_lastMessage = message;
    m_isCallingPluginWndProc = true;

    // If the plug-in doesn't explicitly support changing the pop-up state, we enable
    // popups for all user gestures.
    // Note that we need to pop the state in a timer, because the Flash plug-in 
    // pops up windows in response to a posted message.
    if (m_plugin->pluginFuncs()->version < NPVERS_HAS_POPUPS_ENABLED_STATE &&
        isWindowsMessageUserGesture(message) && !m_popPopupsStateTimer.isActive()) {

        pushPopupsEnabledState(true);

        m_popPopupsStateTimer.startOneShot(0);
    }

    // Call the plug-in's window proc.
    LRESULT result = ::CallWindowProc(m_pluginWndProc, hWnd, message, wParam, lParam);

    m_isCallingPluginWndProc = false;

    return result;
}

void PluginView::updateWindow() const
{
    if (!parent())
        return;

    ASSERT(parent()->isFrameView());
    FrameView* frameView = static_cast<FrameView*>(parent());

    IntRect oldWindowRect = m_windowRect;
    IntRect oldClipRect = m_clipRect;

    m_windowRect = IntRect(frameView->contentsToWindow(frameGeometry().location()), frameGeometry().size());
    m_clipRect = windowClipRect();
    m_clipRect.move(-m_windowRect.x(), -m_windowRect.y());

    if (m_window && (m_windowRect != oldWindowRect || m_clipRect != oldClipRect)) {
        HRGN rgn;

        setCallingPlugin(true);

        // To prevent flashes while scrolling, we disable drawing during the window
        // update process by clipping the window to the zero rect.

        bool clipToZeroRect = !m_plugin->quirks().contains(PluginQuirkDontClipToZeroRectWhenScrolling);

        if (clipToZeroRect) {
            rgn = ::CreateRectRgn(0, 0, 0, 0);
            ::SetWindowRgn(m_window, rgn, FALSE);
        } else {
            rgn = ::CreateRectRgn(m_clipRect.x(), m_clipRect.y(), m_clipRect.right(), m_clipRect.bottom());
            ::SetWindowRgn(m_window, rgn, TRUE);
        }

        if (m_windowRect != oldWindowRect)
            ::MoveWindow(m_window, m_windowRect.x(), m_windowRect.y(), m_windowRect.width(), m_windowRect.height(), TRUE);

        if (clipToZeroRect) {
            rgn = ::CreateRectRgn(m_clipRect.x(), m_clipRect.y(), m_clipRect.right(), m_clipRect.bottom());
            ::SetWindowRgn(m_window, rgn, TRUE);
        }

        setCallingPlugin(false);
    }
}

void PluginView::setFocus()
{
    if (m_window)
        SetFocus(m_window);

    Widget::setFocus();
}

void PluginView::show()
{
    m_isVisible = true;

    if (m_attachedToWindow && m_window)
        ShowWindow(m_window, SW_SHOWNA);

    Widget::show();
}

void PluginView::hide()
{
    m_isVisible = false;

    if (m_attachedToWindow && m_window)
        ShowWindow(m_window, SW_HIDE);

    Widget::hide();
}

void PluginView::paintMissingPluginIcon(GraphicsContext* context, const IntRect& rect)
{
    static Image* nullPluginImage;
    if (!nullPluginImage)
        nullPluginImage = Image::loadPlatformResource("nullPlugin");

    IntRect imageRect(frameGeometry().x(), frameGeometry().y(), nullPluginImage->width(), nullPluginImage->height());

    int xOffset = (frameGeometry().width() - imageRect.width()) / 2;
    int yOffset = (frameGeometry().height() - imageRect.height()) / 2;

    imageRect.move(xOffset, yOffset);

    if (!rect.intersects(imageRect))
        return;

    context->save();
    context->clip(windowClipRect());
    context->drawImage(nullPluginImage, imageRect.location());
    context->restore();
}

bool PluginView::dispatchNPEvent(NPEvent& npEvent)
{
    if (!m_plugin->pluginFuncs()->event)
        return true;

    bool shouldPop = false;

    if (m_plugin->pluginFuncs()->version < NPVERS_HAS_POPUPS_ENABLED_STATE && isWindowsMessageUserGesture(npEvent.event)) {
        pushPopupsEnabledState(true);
        shouldPop = true;
    }

    KJS::JSLock::DropAllLocks dropAllLocks;
    setCallingPlugin(true);
    bool result = m_plugin->pluginFuncs()->event(m_instance, &npEvent);
    setCallingPlugin(false);

    if (shouldPop) 
        popPopupsEnabledState();

    return result;
}

void PluginView::paint(GraphicsContext* context, const IntRect& rect)
{
    if (!m_isStarted) {
        // Draw the "missing plugin" image
        paintMissingPluginIcon(context, rect);
        return;
    }

    if (m_isWindowed || context->paintingDisabled())
        return;

    ASSERT(parent()->isFrameView());
    IntRect rectInWindow = static_cast<FrameView*>(parent())->contentsToWindow(frameGeometry());
    HDC hdc = context->getWindowsContext(rectInWindow, m_isTransparent);
    NPEvent npEvent;

    if (!context->inTransparencyLayer()) {
        // The plugin expects that the passed in DC has window coordinates.
        XFORM transform;
        GetWorldTransform(hdc, &transform);
        transform.eDx = 0;
        transform.eDy = 0;
        SetWorldTransform(hdc, &transform);
    }

    m_npWindow.type = NPWindowTypeDrawable;
    m_npWindow.window = hdc;

    IntPoint p = static_cast<FrameView*>(parent())->contentsToWindow(frameGeometry().location());
    
    WINDOWPOS windowpos;
    memset(&windowpos, 0, sizeof(windowpos));

    windowpos.x = p.x();
    windowpos.y = p.y();
    windowpos.cx = frameGeometry().width();
    windowpos.cy = frameGeometry().height();

    npEvent.event = WM_WINDOWPOSCHANGED;
    npEvent.lParam = reinterpret_cast<uint32>(&windowpos);
    npEvent.wParam = 0;

    dispatchNPEvent(npEvent);

    setNPWindowRect(frameGeometry());

    npEvent.event = WM_PAINT;
    npEvent.wParam = reinterpret_cast<uint32>(hdc);

    // This is supposed to be a pointer to the dirty rect, but it seems that the Flash plugin
    // ignores it so we just pass null.
    npEvent.lParam = 0;

    dispatchNPEvent(npEvent);

    context->releaseWindowsContext(hdc, frameGeometry(), m_isTransparent);
}

void PluginView::handleKeyboardEvent(KeyboardEvent* event)
{
    NPEvent npEvent;

    npEvent.wParam = event->keyCode();    

    if (event->type() == keydownEvent) {
        npEvent.event = WM_KEYDOWN;
        npEvent.lParam = 0;
    } else if (event->type() == keyupEvent) {
        npEvent.event = WM_KEYUP;
        npEvent.lParam = 0x8000;
    }

    KJS::JSLock::DropAllLocks;
    if (!dispatchNPEvent(npEvent))
        event->setDefaultHandled();
}

extern HCURSOR lastSetCursor;
extern bool ignoreNextSetCursor;

void PluginView::handleMouseEvent(MouseEvent* event)
{
    NPEvent npEvent;

    IntPoint p = static_cast<FrameView*>(parent())->contentsToWindow(IntPoint(event->pageX(), event->pageY()));

    npEvent.lParam = MAKELPARAM(p.x(), p.y());
    npEvent.wParam = 0;

    if (event->ctrlKey())
        npEvent.wParam |= MK_CONTROL;
    if (event->shiftKey())
        npEvent.wParam |= MK_SHIFT;

    if (event->type() == mousemoveEvent ||
        event->type() == mouseoutEvent || 
        event->type() == mouseoverEvent) {
        npEvent.event = WM_MOUSEMOVE;
        if (event->buttonDown())
            switch (event->button()) {
                case LeftButton:
                    npEvent.wParam |= MK_LBUTTON;
                    break;
                case MiddleButton:
                    npEvent.wParam |= MK_MBUTTON;
                    break;
                case RightButton:
                    npEvent.wParam |= MK_RBUTTON;
                break;
            }
    }
    else if (event->type() == mousedownEvent) {
        // Focus the plugin
        if (Page* page = m_parentFrame->page())
            page->focusController()->setFocusedFrame(m_parentFrame);
        m_parentFrame->document()->setFocusedNode(m_element);
        switch (event->button()) {
            case 0:
                npEvent.event = WM_LBUTTONDOWN;
                break;
            case 1:
                npEvent.event = WM_MBUTTONDOWN;
                break;
            case 2:
                npEvent.event = WM_RBUTTONDOWN;
                break;
        }
    } else if (event->type() == mouseupEvent) {
        switch (event->button()) {
            case 0:
                npEvent.event = WM_LBUTTONUP;
                break;
            case 1:
                npEvent.event = WM_MBUTTONUP;
                break;
            case 2:
                npEvent.event = WM_RBUTTONUP;
                break;
        }
    } else
        return;

    HCURSOR currentCursor = ::GetCursor();

    KJS::JSLock::DropAllLocks;
    if (!dispatchNPEvent(npEvent))
        event->setDefaultHandled();

    // Currently, Widget::setCursor is always called after this function in EventHandler.cpp
    // and since we don't want that we set ignoreNextSetCursor to true here to prevent that.
    ignoreNextSetCursor = true;     
    lastSetCursor = ::GetCursor();
}

void PluginView::setParent(ScrollView* parent)
{
    Widget::setParent(parent);

    if (parent)
        init();
    else {
        if (!m_window)
            return;

        // If the plug-in window or one of its children have the focus, we need to 
        // clear it to prevent the web view window from being focused because that can
        // trigger a layout while the plugin element is being detached.
        HWND focusedWindow = ::GetFocus();
        if (m_window == focusedWindow || ::IsChild(m_window, focusedWindow))
            ::SetFocus(0);
    }

}

void PluginView::attachToWindow()
{
    if (m_attachedToWindow)
        return;

    m_attachedToWindow = true;
    if (m_isVisible && m_window)
        ShowWindow(m_window, SW_SHOWNA);
}

void PluginView::detachFromWindow()
{
    if (!m_attachedToWindow)
        return;

    if (m_isVisible && m_window)
        ShowWindow(m_window, SW_HIDE);
    m_attachedToWindow = false;
}

void PluginView::setNPWindowRect(const IntRect& rect)
{
    if (!m_isStarted)
        return;

    IntPoint p = static_cast<FrameView*>(parent())->contentsToWindow(rect.location());
    m_npWindow.x = p.x();
    m_npWindow.y = p.y();

    m_npWindow.width = rect.width();
    m_npWindow.height = rect.height();

    m_npWindow.clipRect.left = 0;
    m_npWindow.clipRect.top = 0;
    m_npWindow.clipRect.right = rect.width();
    m_npWindow.clipRect.bottom = rect.height();

    if (m_plugin->pluginFuncs()->setwindow) {
        KJS::JSLock::DropAllLocks dropAllLocks;
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
        setCallingPlugin(false);

        if (!m_isWindowed)
            return;

        ASSERT(m_window);

        WNDPROC currentWndProc = (WNDPROC)GetWindowLongPtr(m_window, GWLP_WNDPROC);
        if (currentWndProc != PluginViewWndProc)
            m_pluginWndProc = (WNDPROC)SetWindowLongPtr(m_window, GWLP_WNDPROC, (LONG)PluginViewWndProc);
    }
}

void PluginView::stop()
{
    if (!m_isStarted)
        return;

    HashSet<RefPtr<PluginStream> > streams = m_streams;
    HashSet<RefPtr<PluginStream> >::iterator end = streams.end();
    for (HashSet<RefPtr<PluginStream> >::iterator it = streams.begin(); it != end; ++it) {
        (*it)->stop();
        disconnectStream((*it).get());
    }

    ASSERT(m_streams.isEmpty());

    m_isStarted = false;

    // Unsubclass the window
    if (m_isWindowed) {
        WNDPROC currentWndProc = (WNDPROC)GetWindowLongPtr(m_window, GWLP_WNDPROC);
        
        if (currentWndProc == PluginViewWndProc)
            SetWindowLongPtr(m_window, GWLP_WNDPROC, (LONG)m_pluginWndProc);
    }

    KJS::JSLock::DropAllLocks dropAllLocks;

    // Clear the window
    m_npWindow.window = 0;
    if (m_plugin->pluginFuncs()->setwindow && !m_plugin->quirks().contains(PluginQuirkDontSetNullWindowHandleOnDestroy)) {
        setCallingPlugin(true);
        m_plugin->pluginFuncs()->setwindow(m_instance, &m_npWindow);
        setCallingPlugin(false);
    }

    // Destroy the plugin
    NPSavedData* savedData = 0;
    setCallingPlugin(true);
    NPError npErr = m_plugin->pluginFuncs()->destroy(m_instance, &savedData);
    setCallingPlugin(false);
    LOG_NPERROR(npErr);

    if (savedData) {
        if (savedData->buf)
            NPN_MemFree(savedData->buf);
        NPN_MemFree(savedData);
    }

    m_instance->pdata = 0;
}

const char* PluginView::userAgentStatic()
{
    return 0;
}

const char* PluginView::userAgent()
{
    if (m_plugin->quirks().contains(PluginQuirkWantsMozillaUserAgent))
        return MozillaUserAgent;

    if (m_userAgent.isNull())
        m_userAgent = m_parentFrame->loader()->userAgent(m_url).utf8();
    return m_userAgent.data();
}

NPError PluginView::handlePostReadFile(Vector<char>& buffer, uint32 len, const char* buf)
{
    String filename(buf, len);

    if (filename.startsWith("file:///"))
        filename = filename.substring(8);

    // Get file info
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (GetFileAttributesExW(filename.charactersWithNullTermination(), GetFileExInfoStandard, &attrs) == 0)
        return NPERR_FILE_NOT_FOUND;

    if (attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        return NPERR_FILE_NOT_FOUND;

    HANDLE fileHandle = CreateFileW(filename.charactersWithNullTermination(), FILE_READ_DATA, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    
    if (fileHandle == INVALID_HANDLE_VALUE)
        return NPERR_FILE_NOT_FOUND;

    buffer.resize(attrs.nFileSizeLow);

    DWORD bytesRead;
    int retval = ReadFile(fileHandle, buffer.data(), attrs.nFileSizeLow, &bytesRead, 0);

    CloseHandle(fileHandle);

    if (retval == 0 || bytesRead != attrs.nFileSizeLow)
        return NPERR_FILE_NOT_FOUND;

    return NPERR_NO_ERROR;
}

NPError PluginView::getValueStatic(NPNVariable variable, void* value)
{
    return NPERR_GENERIC_ERROR;
}

NPError PluginView::getValue(NPNVariable variable, void* value)
{
    switch (variable) {
#if ENABLE(NETSCAPE_PLUGIN_API)
        case NPNVWindowNPObject: {
            if (m_isJavaScriptPaused)
                return NPERR_GENERIC_ERROR;

            NPObject* windowScriptObject = m_parentFrame->windowScriptNPObject();

            // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugin/npruntime.html>
            if (windowScriptObject)
                _NPN_RetainObject(windowScriptObject);

            void** v = (void**)value;
            *v = windowScriptObject;

            return NPERR_NO_ERROR;
        }

        case NPNVPluginElementNPObject: {
            if (m_isJavaScriptPaused)
                return NPERR_GENERIC_ERROR;

            NPObject* pluginScriptObject = 0;

            if (m_element->hasTagName(appletTag) || m_element->hasTagName(embedTag) || m_element->hasTagName(objectTag))
                pluginScriptObject = static_cast<HTMLPlugInElement*>(m_element)->getNPObject();

            // Return value is expected to be retained, as described here: <http://www.mozilla.org/projects/plugin/npruntime.html>
            if (pluginScriptObject)
                _NPN_RetainObject(pluginScriptObject);

            void** v = (void**)value;
            *v = pluginScriptObject;

            return NPERR_NO_ERROR;
        }
#endif

        case NPNVnetscapeWindow: {
            PlatformWidget* w = reinterpret_cast<PlatformWidget*>(value);

            *w = containingWindow();

            return NPERR_NO_ERROR;
        }
        default:
            return NPERR_GENERIC_ERROR;
    }
}

void PluginView::invalidateRect(NPRect* rect)
{
    if (!rect) {
        invalidate();
        return;
    }

    IntRect r(rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top);

    if (m_isWindowed) {
        RECT invalidRect(r);
        InvalidateRect(m_window, &invalidRect, FALSE);
    } else {
        if (m_plugin->quirks().contains(PluginQuirkThrottleInvalidate)) {
            m_invalidRects.append(r);
            if (!m_invalidateTimer.isActive())
                m_invalidateTimer.startOneShot(0.001);
        } else
            Widget::invalidateRect(r);
    }
}

void PluginView::invalidateRegion(NPRegion region)
{
    if (m_isWindowed)
        return;

    RECT r;

    if (GetRgnBox(region, &r) == 0) {
        invalidate();
        return;
    }

    Widget::invalidateRect(r);
}

void PluginView::forceRedraw()
{
    if (m_isWindowed)
        ::UpdateWindow(m_window);
    else
        ::UpdateWindow(containingWindow());
}

PluginView::~PluginView()
{
    stop();

    deleteAllValues(m_requests);

    freeStringArray(m_paramNames, m_paramCount);
    freeStringArray(m_paramValues, m_paramCount);

    if (m_window)
        DestroyWindow(m_window);

    m_parentFrame->cleanupScriptObjectsForPlugin(this);

    if (m_plugin && !m_plugin->quirks().contains(PluginQuirkDontUnloadPlugin))
        m_plugin->unload();
}

void PluginView::init()
{
    if (m_haveInitialized)
        return;
    m_haveInitialized = true;

    if (!m_plugin) {
        ASSERT(m_status == PluginStatusCanNotFindPlugin);
        return;
    }

    if (!m_plugin->load()) {
        m_plugin = 0;
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }

    if (!start()) {
        m_status = PluginStatusCanNotLoadPlugin;
        return;
    }

    if (m_isWindowed) {
        registerPluginView();

        DWORD flags = WS_CHILD;
        if (m_isVisible)
            flags |= WS_VISIBLE;

        m_window = CreateWindowEx(0, kWebPluginViewdowClassName, 0, flags,
                                  0, 0, 0, 0, m_parentFrame->view()->containingWindow(), 0, Page::instanceHandle(), 0);

        // Calling SetWindowLongPtrA here makes the window proc ASCII, which is required by at least
        // the Shockwave Director plug-in.
        ::SetWindowLongPtrA(m_window, GWL_WNDPROC, (LONG)DefWindowProcA);

        SetProp(m_window, kWebPluginViewProperty, this);

        m_npWindow.type = NPWindowTypeWindow;
        m_npWindow.window = m_window;
    } else {
        m_npWindow.type = NPWindowTypeDrawable;
        m_npWindow.window = 0;
    }

    if (!m_plugin->quirks().contains(PluginQuirkDeferFirstSetWindowCall))
        setNPWindowRect(frameGeometry());

    m_status = PluginStatusLoadedSuccessfully;
}

} // namespace WebCore
