/*
* Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef GDIUtilties_h
#define GDIUtilties_h

#include "IntPoint.h"

#include <windows.h>

namespace WebCore {

WEBCORE_EXPORT float deviceScaleFactorForWindow(HWND);

inline LPARAM makeScaledPoint(IntPoint point, float scaleFactor)
{
    float inverseScaleFactor = 1.0f / scaleFactor;
    point.scale(inverseScaleFactor, inverseScaleFactor);
    return MAKELPARAM(point.x(), point.y());
}

inline unsigned short buttonsForEvent(WPARAM wparam)
{
    unsigned short buttons = 0;
    if (wparam & MK_LBUTTON)
        buttons |= 1;
    if (wparam & MK_MBUTTON)
        buttons |= 4;
    if (wparam & MK_RBUTTON)
        buttons |= 2;
    return buttons;
}

inline LONG getDoubleClickTime()
{
    // GetDoubleClickTime() returns 0 in the non-interactive window station on Windows 10 version 2004
    LONG doubleClickTime = GetDoubleClickTime();
    return doubleClickTime ? doubleClickTime : 500;
}

} // namespace WebCore

#endif // GDIUtilties_h
