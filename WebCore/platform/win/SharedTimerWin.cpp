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

#include "Page.h"
#include "Settings.h"
#include "Widget.h"
#include <wtf/Assertions.h>
#include <wtf/CurrentTime.h>

// Note: wx headers set defines that affect the configuration of windows.h
// so we must include the wx header first to get unicode versions of functions,
// etc.
#if PLATFORM(WX)
#include <wx/wx.h>
#endif

#include <windows.h>
#include <mmsystem.h>

#if PLATFORM(WIN)
#include "PluginView.h"
#endif

// These aren't in winuser.h with the MSVS 2003 Platform SDK, 
// so use default values in that case.
#ifndef USER_TIMER_MINIMUM
#define USER_TIMER_MINIMUM 0x0000000A
#endif

#ifndef USER_TIMER_MAXIMUM
#define USER_TIMER_MAXIMUM 0x7FFFFFFF
#endif

#ifndef QS_RAWINPUT
#define QS_RAWINPUT         0x0400
#endif

namespace WebCore {

static UINT timerID;
static void (*sharedTimerFiredFunction)();

static HWND timerWindowHandle = 0;
static UINT timerFiredMessage = 0;
static HANDLE timerQueue;
static HANDLE timer;
static bool highResTimerActive;
static bool processingCustomTimerMessage = false;
static LONG pendingTimers;

const LPCWSTR kTimerWindowClassName = L"TimerWindowClass";
const int timerResolution = 1; // To improve timer resolution, we call timeBeginPeriod/timeEndPeriod with this value to increase timer resolution to 1ms.
const int highResolutionThresholdMsec = 16; // Only activate high-res timer for sub-16ms timers (Windows can fire timers at 16ms intervals without changing the system resolution).
const int stopHighResTimerInMsec = 300; // Stop high-res timer after 0.3 seconds to lessen power consumption (we don't use a smaller time since oscillating between high and low resolution breaks timer accuracy on XP).

enum {
    sharedTimerID = 1000,
    endHighResTimerID = 1001,
};

LRESULT CALLBACK TimerWindowWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
#if PLATFORM(WIN)
    // Windows Media Player has a modal message loop that will deliver messages
    // to us at inappropriate times and we will crash if we handle them when
    // they are delivered. We repost all messages so that we will get to handle
    // them once the modal loop exits.
    if (PluginView::isCallingPlugin()) {
        PostMessage(hWnd, message, wParam, lParam);
        return 0;
    }
#endif

    if (message == timerFiredMessage) {
        InterlockedExchange(&pendingTimers, 0);
        processingCustomTimerMessage = true;
        sharedTimerFiredFunction();
        processingCustomTimerMessage = false;
    } else if (message == WM_TIMER) {
        if (wParam == sharedTimerID) {
            KillTimer(timerWindowHandle, sharedTimerID);
            sharedTimerFiredFunction();
        } else if (wParam == endHighResTimerID) {
            KillTimer(timerWindowHandle, endHighResTimerID);
            highResTimerActive = false;
            timeEndPeriod(timerResolution);
        }
    } else
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
    wcex.hInstance      = Page::instanceHandle();
    wcex.lpszClassName  = kTimerWindowClassName;
    RegisterClassEx(&wcex);

    timerWindowHandle = CreateWindow(kTimerWindowClassName, 0, 0,
       CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, HWND_MESSAGE, 0, Page::instanceHandle(), 0);
    timerFiredMessage = RegisterWindowMessage(L"com.apple.WebKit.TimerFired");
}

void setSharedTimerFiredFunction(void (*f)())
{
    sharedTimerFiredFunction = f;
}

static void NTAPI queueTimerProc(PVOID, BOOLEAN)
{
    if (InterlockedIncrement(&pendingTimers) == 1)
        PostMessage(timerWindowHandle, timerFiredMessage, 0, 0);
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

    initializeOffScreenTimerWindow();
    bool timerSet = false;

    if (Settings::shouldUseHighResolutionTimers()) {
        if (interval < highResolutionThresholdMsec) {
            if (!highResTimerActive) {
                highResTimerActive = true;
                timeBeginPeriod(timerResolution);
            }
            SetTimer(timerWindowHandle, endHighResTimerID, stopHighResTimerInMsec, 0);
        }

        DWORD queueStatus = LOWORD(GetQueueStatus(QS_PAINT | QS_MOUSEBUTTON | QS_KEY | QS_RAWINPUT));

        // Win32 has a tri-level queue with application messages > user input > WM_PAINT/WM_TIMER.

        // If the queue doesn't contains input events, we use a higher priorty timer event posting mechanism.
        if (!(queueStatus & (QS_MOUSEBUTTON | QS_KEY | QS_RAWINPUT))) {
            if (intervalInMS < USER_TIMER_MINIMUM && !processingCustomTimerMessage && !(queueStatus & QS_PAINT)) {
                // Call PostMessage immediately if the timer is already expired, unless a paint is pending.
                // (we prioritize paints over timers)
                if (InterlockedIncrement(&pendingTimers) == 1)
                    PostMessage(timerWindowHandle, timerFiredMessage, 0, 0);
                timerSet = true;
            } else {
                // Otherwise, delay the PostMessage via a CreateTimerQueueTimer
                if (!timerQueue)
                    timerQueue = CreateTimerQueue();
                if (timer)
                    DeleteTimerQueueTimer(timerQueue, timer, 0);
                timerSet = CreateTimerQueueTimer(&timer, timerQueue, queueTimerProc, 0, intervalInMS, 0, WT_EXECUTEINTIMERTHREAD | WT_EXECUTEONLYONCE);
            }
        }
    }

    if (timerSet) {
        if (timerID) {
            KillTimer(timerWindowHandle, timerID);
            timerID = 0;
        }
    } else {
        timerID = SetTimer(timerWindowHandle, sharedTimerID, intervalInMS, 0);
        timer = 0;
    }
}

void stopSharedTimer()
{
    if (timerQueue && timer) {
        DeleteTimerQueueTimer(timerQueue, timer, 0);
        timer = 0;
    }

    if (timerID) {
        KillTimer(timerWindowHandle, timerID);
        timerID = 0;
    }
}

}
