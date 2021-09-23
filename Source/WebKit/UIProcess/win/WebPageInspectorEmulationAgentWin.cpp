/*
 * Copyright (C) 2020 Microsoft Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebPageInspectorEmulationAgent.h"
#include "WebPageProxy.h"

namespace WebKit {

void WebPageInspectorEmulationAgent::platformSetSize(int width, int height, Function<void (const String& error)>&& callback)
{
    HWND viewHwnd = m_page.viewWidget();
    HWND windowHwnd = GetAncestor(viewHwnd, GA_ROOT);
    RECT viewRect;
    RECT windowRect;

    if (!windowHwnd || !GetWindowRect(windowHwnd, &windowRect)) {
        callback("Could not retrieve window size");
        return;
    }
    if (!GetWindowRect(viewHwnd, &viewRect)) {
        callback("Could retrieve view size");
        return;
    }

    width += windowRect.right - windowRect.left - viewRect.right + viewRect.left;
    height += windowRect.bottom - windowRect.top - viewRect.bottom + viewRect.top;

    if (!SetWindowPos(windowHwnd, 0, 0, 0, width, height, SWP_NOCOPYBITS | SWP_NOSENDCHANGING | SWP_NOMOVE)) {
        callback("Could not resize window");
        return;
    }
    callback(String());
}

} // namespace WebKit
