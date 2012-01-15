/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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
#include "WebScreenInfoFactory.h"

#include "HWndDC.h"
#include "WebScreenInfo.h"

#include <windows.h>

namespace WebKit {

static WebRect toWebRect(const RECT& input)
{
    WebRect output;
    output.x = input.left;
    output.y = input.top;
    output.width = input.right - input.left;
    output.height = input.bottom - input.top;
    return output;
}

WebScreenInfo WebScreenInfoFactory::screenInfo(HWND window)
{
    HMONITOR monitor = MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFOEX monitorInfo;
    monitorInfo.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(monitor, &monitorInfo);

    DEVMODE devMode;
    devMode.dmSize = sizeof(devMode);
    devMode.dmDriverExtra = 0;
    EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &devMode);

    WebCore::HWndDC hdc(0);
    ASSERT(hdc);

    WebScreenInfo results;
    results.horizontalDPI = GetDeviceCaps(hdc, LOGPIXELSX);
    results.verticalDPI = GetDeviceCaps(hdc, LOGPIXELSY);
    results.depth = devMode.dmBitsPerPel;
    results.depthPerComponent = devMode.dmBitsPerPel / 3;  // Assumes RGB
    results.isMonochrome = devMode.dmColor == DMCOLOR_MONOCHROME;
    results.rect = toWebRect(monitorInfo.rcMonitor);
    results.availableRect = toWebRect(monitorInfo.rcWork);
    return results;
}

} // namespace WebKit
