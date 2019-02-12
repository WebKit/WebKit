/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <pal/spi/cg/CoreGraphicsSPI.h>

#if USE(APPLE_INTERNAL_SDK)

#include <ApplicationServices/ApplicationServicesPriv.h>

#else

typedef CF_ENUM(SInt32, CoreCursorType) {
    kCoreCursorFirstCursor = 0,
    kCoreCursorArrow = kCoreCursorFirstCursor,
    kCoreCursorIBeam,
    kCoreCursorMakeAlias,
    kCoreCursorNotAllowed,
    kCoreCursorBusyButClickable,
    kCoreCursorCopy,
    kCoreCursorScreenShotSelection = 7,
    kCoreCursorScreenShotSelectionToClip,
    kCoreCursorScreenShotWindow,
    kCoreCursorScreenShotWindowToClip,
    kCoreCursorClosedHand,
    kCoreCursorOpenHand,
    kCoreCursorPointingHand,
    kCoreCursorCountingUpHand,
    kCoreCursorCountingDownHand,
    kCoreCursorCountingUpAndDownHand,
    kCoreCursorResizeLeft,
    kCoreCursorResizeRight,
    kCoreCursorResizeLeftRight,
    kCoreCursorCross,
    kCoreCursorResizeUp,
    kCoreCursorResizeDown,
    kCoreCursorResizeUpDown,
    kCoreCursorContextualMenu,
    kCoreCursorPoof,
    kCoreCursorIBeamVertical,
    kCoreCursorWindowResizeEast,
    kCoreCursorWindowResizeEastWest,
    kCoreCursorWindowResizeNorthEast,
    kCoreCursorWindowResizeNorthEastSouthWest,
    kCoreCursorWindowResizeNorth,
    kCoreCursorWindowResizeNorthSouth,
    kCoreCursorWindowResizeNorthWest,
    kCoreCursorWindowResizeNorthWestSouthEast,
    kCoreCursorWindowResizeSouthEast,
    kCoreCursorWindowResizeSouth,
    kCoreCursorWindowResizeSouthWest,
    kCoreCursorWindowResizeWest,
    kCoreCursorWindowMove,
    kCoreCursorHelp,
    kCoreCursorCell,
    kCoreCursorZoomIn,
    kCoreCursorZoomOut,
    kCoreCursorLastCursor = kCoreCursorZoomOut
};

enum {
    kCoreDragImageSpecVersionOne = 1,
};

struct CoreDragImageSpec {
    UInt32 version;
    SInt32 pixelsWide;
    SInt32 pixelsHigh;
    SInt32 bitsPerSample;
    SInt32 samplesPerPixel;
    SInt32 bitsPerPixel;
    SInt32 bytesPerRow;
    Boolean isPlanar;
    Boolean hasAlpha;
    const UInt8* data[5];
};

enum {
    kMSHDoNotCreateSendRightOption = 0x4,
};

#endif

typedef UInt32 MSHCreateOptions;
typedef const struct __AXTextMarker* AXTextMarkerRef;
typedef const struct __AXTextMarkerRange* AXTextMarkerRangeRef;
typedef struct CoreDragImageSpec CoreDragImageSpec;
typedef struct OpaqueCoreDrag* CoreDragRef;

WTF_EXTERN_C_BEGIN

AXTextMarkerRangeRef AXTextMarkerRangeCreate(CFAllocatorRef, AXTextMarkerRef startMarker, AXTextMarkerRef endMarker);
AXTextMarkerRef AXTextMarkerCreate(CFAllocatorRef, const UInt8* bytes, CFIndex length);
AXTextMarkerRef AXTextMarkerRangeCopyStartMarker(AXTextMarkerRangeRef);
AXTextMarkerRef AXTextMarkerRangeCopyEndMarker(AXTextMarkerRangeRef);
CFIndex AXTextMarkerGetLength(AXTextMarkerRef);
CFRunLoopSourceRef MSHCreateMIGServerSource(CFAllocatorRef, CFIndex order, mig_subsystem_t sub_system, MSHCreateOptions, mach_port_t, void* user_data);
CFTypeID AXTextMarkerGetTypeID();
CFTypeID AXTextMarkerRangeGetTypeID();
CoreDragRef CoreDragGetCurrentDrag();
OSStatus CoreDragSetImage(CoreDragRef, CGPoint imageOffset, CoreDragImageSpec*, CGSRegionObj imageShape, float overallAlpha);
const UInt8* AXTextMarkerGetBytePtr(AXTextMarkerRef);
bool _AXUIElementRequestServicedBySecondaryAXThread(void);
OSStatus SetApplicationIsDaemon(Boolean);

WTF_EXTERN_C_END
