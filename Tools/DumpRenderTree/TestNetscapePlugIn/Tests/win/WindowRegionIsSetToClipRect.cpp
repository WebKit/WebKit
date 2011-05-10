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

#include "WindowedPluginTest.h"

#include "PluginObject.h"

using namespace std;

// The plugin's window's window region should be set to the plugin's clip rect.

class WindowRegionIsSetToClipRect : public WindowedPluginTest {
public:
    WindowRegionIsSetToClipRect(NPP, const string& identifier);

private:
    struct ScriptObject : Object<ScriptObject> {
        bool hasMethod(NPIdentifier);
        bool invoke(NPIdentifier, const NPVariant*, uint32_t, NPVariant*);
    };

    static const UINT_PTR triggerPaintTimerID = 1;

    void startTest();
    void finishTest();
    void checkWindowRegion();

    void showTestHarnessWindowIfNeeded();
    void hideTestHarnessWindowIfNeeded();

    // WindowedPluginTest
    virtual LRESULT wndProc(UINT message, WPARAM, LPARAM, bool& handled);

    // PluginTest
    virtual NPError NPP_GetValue(NPPVariable, void*);

    bool m_didCheckWindowRegion;
    bool m_testHarnessWindowWasVisible;
};

static PluginTest::Register<WindowRegionIsSetToClipRect> registrar("window-region-is-set-to-clip-rect");

WindowRegionIsSetToClipRect::WindowRegionIsSetToClipRect(NPP npp, const string& identifier)
    : WindowedPluginTest(npp, identifier)
    , m_didCheckWindowRegion(false)
    , m_testHarnessWindowWasVisible(false)
{
}

void WindowRegionIsSetToClipRect::startTest()
{
    // In WebKit1, our window's window region will be set immediately. In WebKit2, it won't be set
    // until the UI process paints. Since the UI process will also show our window when it paints,
    // we can detect when the paint occurs (and thus when our window region should be set) by
    // starting with our plugin element hidden, then making it visible and waiting for a
    // WM_WINDOWPOSCHANGED event to tell us our window has been shown.

    waitUntilDone();

    // If the test harness window isn't visible, we might not receive a WM_WINDOWPOSCHANGED message
    // when our window is made visible. So we temporarily show the test harness window during this test.
    showTestHarnessWindowIfNeeded();

    // Make our window visible. (In WebKit2, this won't take effect immediately.)
    executeScript("document.getElementsByTagName('embed')[0].style.visibility = 'visible';");

    // We trigger a UI process paint after a slight delay to ensure that the UI process has
    // received the "make the plugin window visible" message before it paints.
    // FIXME: It would be nice to have a way to guarantee that the UI process had received that
    // message before we triggered a paint. Hopefully that would let us get rid of this semi-
    // arbitrary timeout.
    ::SetTimer(window(), triggerPaintTimerID, 250, 0);
}

void WindowRegionIsSetToClipRect::finishTest()
{
    checkWindowRegion();
    hideTestHarnessWindowIfNeeded();
    notifyDone();
}

void WindowRegionIsSetToClipRect::checkWindowRegion()
{
    if (m_didCheckWindowRegion)
        return;
    m_didCheckWindowRegion = true;

    RECT regionRect;
    if (::GetWindowRgnBox(window(), &regionRect) == ERROR) {
        log("::GetWindowRgnBox failed, or window has no window region");
        return;
    }

    // This expected rect is based on the layout of window-region-is-set-to-clip-rect.html.
    RECT expectedRect = { 50, 50, 100, 100 };
    if (!::EqualRect(&regionRect, &expectedRect)) {
        log("Expected region rect {left=%u, top=%u, right=%u, bottom=%u}, but got {left=%d, top=%d, right=%d, bottom=%d}", expectedRect.left, expectedRect.top, expectedRect.right, expectedRect.bottom, regionRect.left, regionRect.top, regionRect.right, regionRect.bottom);
        return;
    }

    log("PASS: Plugin's window's window region has been set as expected");

    // While we're here, check that our window class doesn't have the CS_PARENTDC style, which
    // defeats clipping by ignoring the window region and always clipping to the parent window.
    // FIXME: It would be nice to have a pixel test that shows that we're
    // getting clipped correctly, but unfortunately window regions are ignored
    // during WM_PRINT (see <http://webkit.org/b/49034>).
    wchar_t className[512];
    if (!::GetClassNameW(window(), className, _countof(className))) {
        log("::GetClassName failed with error %u", ::GetLastError());
        return;
    }

#ifdef DEBUG_ALL
    const wchar_t webKitDLLName[] = L"WebKit_debug.dll";
#else
    const wchar_t webKitDLLName[] = L"WebKit.dll";
#endif
    HMODULE webKitModule = ::GetModuleHandleW(webKitDLLName);
    if (!webKitModule) {
        log("::GetModuleHandleW failed with error %u", ::GetLastError());
        return;
    }

    WNDCLASSW wndClass;
    if (!::GetClassInfoW(webKitModule, className, &wndClass)) {
        log("::GetClassInfoW failed with error %u", ::GetLastError());
        return;
    }

    if (wndClass.style & CS_PARENTDC)
        log("FAIL: Plugin's window's class has the CS_PARENTDC style, which will defeat clipping");
    else
        log("PASS: Plugin's window's class does not have the CS_PARENTDC style");
}

void WindowRegionIsSetToClipRect::showTestHarnessWindowIfNeeded()
{
    HWND testHarnessWindow = this->testHarnessWindow();
    m_testHarnessWindowWasVisible = ::IsWindowVisible(testHarnessWindow);
    if (m_testHarnessWindowWasVisible)
        return;
    ::ShowWindow(testHarnessWindow, SW_SHOWNA);
}

void WindowRegionIsSetToClipRect::hideTestHarnessWindowIfNeeded()
{
    if (m_testHarnessWindowWasVisible)
        return;
    ::ShowWindow(testHarnessWindow(), SW_HIDE);
}

LRESULT WindowRegionIsSetToClipRect::wndProc(UINT message, WPARAM wParam, LPARAM lParam, bool& handled)
{
    switch (message) {
    case WM_TIMER:
        if (wParam != triggerPaintTimerID)
            break;
        handled = true;
        ::KillTimer(window(), wParam);
        // Tell the UI process to paint.
        ::PostMessageW(::GetParent(window()), WM_PAINT, 0, 0);
        break;
    case WM_WINDOWPOSCHANGED: {
        WINDOWPOS* windowPos = reinterpret_cast<WINDOWPOS*>(lParam);
        if (!(windowPos->flags & SWP_SHOWWINDOW))
            break;
        finishTest();
        break;
    }

    }

    return 0;
}

NPError WindowRegionIsSetToClipRect::NPP_GetValue(NPPVariable variable, void* value)
{
    if (variable != NPPVpluginScriptableNPObject)
        return NPERR_GENERIC_ERROR;

    *static_cast<NPObject**>(value) = ScriptObject::create(this);

    return NPERR_NO_ERROR;
}

bool WindowRegionIsSetToClipRect::ScriptObject::hasMethod(NPIdentifier methodName)
{
    return methodName == pluginTest()->NPN_GetStringIdentifier("startTest");
}

bool WindowRegionIsSetToClipRect::ScriptObject::invoke(NPIdentifier identifier, const NPVariant*, uint32_t, NPVariant*)
{
    assert(identifier == pluginTest()->NPN_GetStringIdentifier("startTest"));
    static_cast<WindowRegionIsSetToClipRect*>(pluginTest())->startTest();
    return true;
}
