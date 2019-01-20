/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#pragma once

#ifndef __LP64__

#if USE(APPLE_INTERNAL_SDK)

#include <ApplicationServices/ApplicationServicesPriv.h>

#else

#define MacSetRect SetRect
#define MacSetRectRgn SetRectRgn
#define MacUnionRgn UnionRgn

enum {
    blackColor = 33,
    whiteColor = 30,
    greenColor = 341,
};

enum {
    kBitsProc = 8,
};

#endif

typedef long QDDrawingType;

WTF_EXTERN_C_BEGIN

Boolean EmptyRgn(RgnHandle);
OSStatus CreateCGContextForPort(CGrafPtr, CGContextRef*);
OSStatus SyncCGContextOriginWithPort(CGContextRef, CGrafPtr);
PixMapHandle GetPortPixMap(CGrafPtr);
QDErr NewGWorldFromPtr(GWorldPtr*, UInt32, const Rect*, CTabHandle, GDHandle, GWorldFlags, Ptr, SInt32);
Rect* GetPortBounds(CGrafPtr, Rect*);
Rect* GetRegionBounds(RgnHandle, Rect*);
RgnHandle GetPortClipRegion(CGrafPtr, RgnHandle);
RgnHandle GetPortVisibleRegion(CGrafPtr, RgnHandle);
RgnHandle NewRgn(void);
void BackColor(long);
void CallDrawingNotifications(CGrafPtr, const Rect*, QDDrawingType);
void DisposeGWorld(GWorldPtr);
void DisposeRgn(RgnHandle);
void ForeColor(long);
void GetGWorld(CGrafPtr*, GDHandle*);
void GetPort(GrafPtr*);
void GlobalToLocal(Point*);
void MacSetRect(Rect*, short, short, short, short);
void MacSetRectRgn(RgnHandle, short, short, short, short);
void MacUnionRgn(RgnHandle, RgnHandle, RgnHandle);
void MovePortTo(short, short);
void OffsetRect(Rect*, short, short);
void OffsetRgn(RgnHandle, short, short);
void PaintRect(const Rect*);
void PenNormal(void);
void PortSize(short, short);
void RectRgn(RgnHandle, const Rect*);
void SectRgn(RgnHandle, RgnHandle, RgnHandle);
void SetGWorld(CGrafPtr, GDHandle);
void SetOrigin(short, short);
void SetPort(GrafPtr);
void SetPortClipRegion(CGrafPtr, RgnHandle);
void SetPortVisibleRegion(CGrafPtr, RgnHandle);

WTF_EXTERN_C_END

#endif
