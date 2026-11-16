/*
 * Copyright (C) 2004-2020 Apple Inc. All rights reserved.
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

#include <WebCore/IntPoint.h>
#include <WebCore/NativeImage.h>
#include <wtf/Assertions.h>
#include <wtf/Platform.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

#if PLATFORM(WIN)
typedef struct HICON__* HICON;
typedef HICON HCURSOR;
#include <wtf/RefCounted.h>
#elif PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#endif

#if HAVE(NSCURSOR)
OBJC_CLASS NSCursor;
#endif

#if PLATFORM(WIN)
typedef struct HICON__ *HICON;
typedef HICON HCURSOR;
#endif

namespace WebCore {

class ShareableBitmap;

#if PLATFORM(WIN)

class SharedCursor : public RefCounted<SharedCursor> {
public:
    static Ref<SharedCursor> create(HCURSOR);
    WEBCORE_EXPORT ~SharedCursor();
    HCURSOR nativeCursor() const { return m_nativeCursor; }

private:
    SharedCursor(HCURSOR);
    HCURSOR m_nativeCursor;
};

#endif

#if PLATFORM(WIN)
using PlatformCursor = RefPtr<SharedCursor>;
#elif HAVE(NSCURSOR)
using PlatformCursor = NSCursor *;
#else
using PlatformCursor = void*;
#endif

enum class PlatformCursorType : uint8_t {
    Invalid,
    Pointer,
    Cross,
    Hand,
    IBeam,
    Wait,
    Help,
    EastResize,
    NorthResize,
    NorthEastResize,
    NorthWestResize,
    SouthResize,
    SouthEastResize,
    SouthWestResize,
    WestResize,
    NorthSouthResize,
    EastWestResize,
    NorthEastSouthWestResize,
    NorthWestSouthEastResize,
    ColumnResize,
    RowResize,
    MiddlePanning,
    EastPanning,
    NorthPanning,
    NorthEastPanning,
    NorthWestPanning,
    SouthPanning,
    SouthEastPanning,
    SouthWestPanning,
    WestPanning,
    Move,
    VerticalText,
    Cell,
    ContextMenu,
    Alias,
    Progress,
    NoDrop,
    Copy,
    None,
    NotAllowed,
    ZoomIn,
    ZoomOut,
    Grab,
    Grabbing,
    Custom
};

class Cursor {
    WTF_MAKE_TZONE_ALLOCATED(Cursor);
public:
    using Type = PlatformCursorType;

    struct CustomCursorIPCData {
        Ref<ShareableBitmap> image;
        IntPoint hotSpot;
#if ENABLE(MOUSE_CURSOR_SCALE)
        float scaleFactor { 0 };
#endif
    };
    using IPCData = Variant<Type /* Non custom type */, std::optional<CustomCursorIPCData>>;

    Cursor() = default;
    WEBCORE_EXPORT static std::optional<Cursor> fromIPCData(IPCData&&);

    NODELETE WEBCORE_EXPORT static const Cursor& fromType(Cursor::Type);

    WEBCORE_EXPORT Cursor(RefPtr<NativeImage>&&, const IntPoint& hotSpot);

#if ENABLE(MOUSE_CURSOR_SCALE)
    // Hot spot is in image pixels.
    WEBCORE_EXPORT Cursor(RefPtr<NativeImage>&&, const IntPoint& hotSpot, float imageScaleFactor);
#endif

    NODELETE explicit Cursor(Type);

    WEBCORE_EXPORT IPCData ipcData() const;

    Type type() const;
    RefPtr<NativeImage> image() const { return m_image; }
    const IntPoint& hotSpot() const LIFETIME_BOUND { return m_hotSpot; }

#if ENABLE(MOUSE_CURSOR_SCALE)
    // Image scale in image pixels per logical (UI) pixel.
    float imageScaleFactor() const { return m_imageScaleFactor; }
#endif

    WEBCORE_EXPORT PlatformCursor platformCursor() const;

    WEBCORE_EXPORT void setAsPlatformCursor() const;

private:
    void ensurePlatformCursor() const;

    Type m_type { Type::Invalid };
    RefPtr<NativeImage> m_image;
    IntPoint m_hotSpot;

#if ENABLE(MOUSE_CURSOR_SCALE)
    float m_imageScaleFactor { 1 };
#endif

#if !HAVE(NSCURSOR)
    mutable PlatformCursor m_platformCursor { nullptr };
#else
    mutable RetainPtr<NSCursor> m_platformCursor;
#endif

};

NODELETE IntPoint determineHotSpot(const IntSize& imageSize, std::optional<IntPoint> specifiedHotSpot, std::optional<IntPoint> intrinsicHotSpot = std::nullopt);

NODELETE WEBCORE_EXPORT const Cursor& pointerCursor();
NODELETE const Cursor& crossCursor();
NODELETE WEBCORE_EXPORT const Cursor& handCursor();
NODELETE const Cursor& moveCursor();
NODELETE WEBCORE_EXPORT const Cursor& iBeamCursor();
NODELETE const Cursor& waitCursor();
NODELETE const Cursor& helpCursor();
NODELETE const Cursor& eastResizeCursor();
NODELETE const Cursor& northResizeCursor();
NODELETE const Cursor& northEastResizeCursor();
NODELETE const Cursor& northWestResizeCursor();
NODELETE const Cursor& southResizeCursor();
NODELETE const Cursor& southEastResizeCursor();
NODELETE const Cursor& southWestResizeCursor();
NODELETE const Cursor& westResizeCursor();
NODELETE const Cursor& northSouthResizeCursor();
NODELETE const Cursor& eastWestResizeCursor();
NODELETE const Cursor& northEastSouthWestResizeCursor();
NODELETE const Cursor& northWestSouthEastResizeCursor();
NODELETE const Cursor& columnResizeCursor();
NODELETE const Cursor& rowResizeCursor();
NODELETE const Cursor& middlePanningCursor();
NODELETE const Cursor& eastPanningCursor();
NODELETE const Cursor& northPanningCursor();
NODELETE const Cursor& northEastPanningCursor();
NODELETE const Cursor& northWestPanningCursor();
NODELETE const Cursor& southPanningCursor();
NODELETE const Cursor& southEastPanningCursor();
NODELETE const Cursor& southWestPanningCursor();
NODELETE const Cursor& westPanningCursor();
NODELETE const Cursor& verticalTextCursor();
NODELETE const Cursor& cellCursor();
NODELETE const Cursor& contextMenuCursor();
NODELETE const Cursor& noDropCursor();
NODELETE const Cursor& notAllowedCursor();
NODELETE const Cursor& progressCursor();
NODELETE const Cursor& aliasCursor();
NODELETE const Cursor& zoomInCursor();
NODELETE const Cursor& zoomOutCursor();
NODELETE const Cursor& copyCursor();
NODELETE const Cursor& noneCursor();
NODELETE const Cursor& grabCursor();
NODELETE const Cursor& grabbingCursor();

inline Cursor::Type Cursor::type() const
{
    ASSERT(m_type > Type::Invalid);
    ASSERT(m_type <= Type::Custom);
    return m_type;
}

} // namespace WebCore
