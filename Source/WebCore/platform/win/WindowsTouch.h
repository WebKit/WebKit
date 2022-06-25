/*
 * Copyright (C) 2009, 2015 Apple Inc.  All rights reserved.
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

#pragma once

/*
 * The following constants are used to determine multitouch and gesture behavior
 * for Windows 7. For more information, see:
 * http://msdn.microsoft.com/en-us/library/dd562197(VS.85).aspx
 */

// Value used in WebViewWndProc for Gestures
#define WM_GESTURE 0x0119
#define WM_GESTURENOTIFY 0x011A

// Gesture Information Flags
#define GF_BEGIN 0x00000001
#define GF_INERTIA 0x00000002
#define GF_END 0x00000004

// Gesture IDs
#define GID_BEGIN 1
#define GID_END 2
#define GID_ZOOM 3
#define GID_PAN 4
#define GID_ROTATE 5
#define GID_TWOFINGERTAP 6
#define GID_PRESSANDTAP 7
#define GID_ROLLOVER GID_PRESSANDTAP

// Zoom Gesture Confiration Flags
#define GC_ZOOM 0x00000001

// Pan Gesture Configuration Flags
#define GC_PAN 0x00000001
#define GC_PAN_WITH_SINGLE_FINGER_VERTICALLY 0x00000002
#define GC_PAN_WITH_SINGLE_FINGER_HORIZONTALLY 0x00000004
#define GC_PAN_WITH_GUTTER 0x00000008
#define GC_PAN_WITH_INERTIA 0x00000010

// Rotate Gesture Configuration Flags
#define GC_ROTATE 0x00000001

// Two finger tap configuration flags
#define GC_TWOFINGERTAP 0x00000001

// Press and tap Configuration Flags
#define GC_PRESSANDTAP 0x00000001
#define GC_ROLLOVER GC_PRESSANDTAP
