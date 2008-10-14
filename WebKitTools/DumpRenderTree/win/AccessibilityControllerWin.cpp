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
#include "AccessibilityController.h"

#include "AccessibilityUIElement.h"
#include "DumpRenderTree.h"
#include <JavaScriptCore/Assertions.h>
#include <WebCore/COMPtr.h>
#include <WebKit/WebKit.h>
#include <oleacc.h>

AccessibilityController::AccessibilityController()
{
}

AccessibilityController::~AccessibilityController()
{
}

AccessibilityUIElement AccessibilityController::focusedElement()
{
    COMPtr<IAccessible> rootAccessible = rootElement().platformUIElement();

    VARIANT vFocus;
    if (FAILED(rootAccessible->get_accFocus(&vFocus)))
        return 0;

    if (V_VT(&vFocus) == VT_I4) {
        ASSERT(V_I4(&vFocus) == CHILDID_SELF);
        // The root accessible object is the focused object.
        return rootAccessible;
    }

    ASSERT(V_VT(&vFocus) == VT_DISPATCH);
    // We have an IDispatch; query for IAccessible.
    return COMPtr<IAccessible>(Query, V_DISPATCH(&vFocus));
}

AccessibilityUIElement AccessibilityController::rootElement()
{
    COMPtr<IWebView> view;
    if (FAILED(frame->webView(&view)))
        return 0;

    COMPtr<IWebViewPrivate> viewPrivate(Query, view);
    if (!viewPrivate)
        return 0;

    HWND webViewWindow;
    if (FAILED(viewPrivate->viewWindow((OLE_HANDLE*)&webViewWindow)))
        return 0;

    // Get the root accessible object by querying for the accessible object for the
    // WebView's window.
    COMPtr<IAccessible> rootAccessible;
    if (FAILED(AccessibleObjectFromWindow(webViewWindow, static_cast<DWORD>(OBJID_CLIENT), __uuidof(IAccessible), reinterpret_cast<void**>(&rootAccessible))))
        return 0;

    return rootAccessible;
}
