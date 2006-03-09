/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "SharedTimer.h"

#include "SystemTime.h"
#include "Widget.h"
#include <kxmlcore/Assertions.h>
#include <windows.h>

namespace WebCore {

static UINT timerID;
static void (*sharedTimerFiredFunction)();

static HWND timerWindowHandle = 0;
static UINT timerFiredMessage = 0;
const LPCWSTR kTimerWindowClassName = L"TimerWindowClass";

LRESULT CALLBACK TimerWindowWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == timerFiredMessage)
        sharedTimerFiredFunction();
    else
        return DefWindowProc(hWnd, message, wParam, lParam);
    return 0;
}

static void initializeOffScreenTimerWindow()
{
    if (timerWindowHandle)
        return;
    
    WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(WNDCLASSEX));
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc    = TimerWindowWndProc;
    wcex.hInstance      = Widget::instanceHandle;
    wcex.lpszClassName  = kTimerWindowClassName;
    RegisterClassEx(&wcex);

    timerWindowHandle = CreateWindow(kTimerWindowClassName, 0, 0,
       CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, HWND_MESSAGE, 0, Widget::instanceHandle, 0);
    timerFiredMessage = RegisterWindowMessage(L"com.apple.WebKit.TimerFired");
}

void setSharedTimerFiredFunction(void (*f)())
{
    sharedTimerFiredFunction = f;
}

static void CALLBACK timerFired(HWND, UINT, UINT_PTR, DWORD)
{
    sharedTimerFiredFunction();
}

void setSharedTimerFireTime(double fireTime)
{
    ASSERT(sharedTimerFiredFunction);

    double interval = fireTime - currentTime();
    unsigned intervalInMS;
    if (interval < 0)
        intervalInMS = 0;
    else {
        interval *= 1000;
        if (interval > USER_TIMER_MAXIMUM)
            intervalInMS = USER_TIMER_MAXIMUM;
        else
            intervalInMS = (unsigned)interval;
    }

    if (timerID)
        KillTimer(0, timerID);

    if (intervalInMS == 0) {
        timerID = 0;
        // Windows SetTimer does not allow timeouts smaller than 10ms (USER_TIMER_MINIMUM)
        initializeOffScreenTimerWindow();
        PostMessage(timerWindowHandle, timerFiredMessage, 0, 0);
    } else
        // FIXME: 1-9ms timeouts may fire too late.
        timerID = SetTimer(0, 0, intervalInMS, timerFired);
}

void stopSharedTimer()
{
    if (timerID) {
        KillTimer(0, timerID);
        timerID = 0;
    }
}

}
