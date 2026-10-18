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

    WEBCORE_EXPORT static const Cursor& LEGACY_NODELETE fromType(Cursor::Type);

    WEBCORE_EXPORT Cursor(RefPtr<NativeImage>&&, const IntPoint& hotSpot);

#if ENABLE(MOUSE_CURSOR_SCALE)
    // Hot spot is in image pixels.
    WEBCORE_EXPORT Cursor(RefPtr<NativeImage>&&, const IntPoint& hotSpot, float imageScaleFactor);
#endif

    explicit Cursor(Type);

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

IntPoint LEGACY_NODELETE determineHotSpot(const IntSize& imageSize, std::optional<IntPoint> specifiedHotSpot, std::optional<IntPoint> intrinsicHotSpot = std::nullopt);

WEBCORE_EXPORT const Cursor& LEGACY_NODELETE pointerCursor();
const Cursor& LEGACY_NODELETE crossCursor();
WEBCORE_EXPORT const Cursor& LEGACY_NODELETE handCursor();
const Cursor& LEGACY_NODELETE moveCursor();
WEBCORE_EXPORT const Cursor& LEGACY_NODELETE iBeamCursor();
const Cursor& LEGACY_NODELETE waitCursor();
const Cursor& LEGACY_NODELETE helpCursor();
const Cursor& LEGACY_NODELETE eastResizeCursor();
const Cursor& LEGACY_NODELETE northResizeCursor();
const Cursor& LEGACY_NODELETE northEastResizeCursor();
const Cursor& LEGACY_NODELETE northWestResizeCursor();
const Cursor& LEGACY_NODELETE southResizeCursor();
const Cursor& LEGACY_NODELETE southEastResizeCursor();
const Cursor& LEGACY_NODELETE southWestResizeCursor();
const Cursor& LEGACY_NODELETE westResizeCursor();
const Cursor& LEGACY_NODELETE northSouthResizeCursor();
const Cursor& LEGACY_NODELETE eastWestResizeCursor();
const Cursor& LEGACY_NODELETE northEastSouthWestResizeCursor();
const Cursor& LEGACY_NODELETE northWestSouthEastResizeCursor();
const Cursor& LEGACY_NODELETE columnResizeCursor();
const Cursor& LEGACY_NODELETE rowResizeCursor();
const Cursor& LEGACY_NODELETE middlePanningCursor();
const Cursor& LEGACY_NODELETE eastPanningCursor();
const Cursor& LEGACY_NODELETE northPanningCursor();
const Cursor& LEGACY_NODELETE northEastPanningCursor();
const Cursor& LEGACY_NODELETE northWestPanningCursor();
const Cursor& LEGACY_NODELETE southPanningCursor();
const Cursor& LEGACY_NODELETE southEastPanningCursor();
const Cursor& LEGACY_NODELETE southWestPanningCursor();
const Cursor& LEGACY_NODELETE westPanningCursor();
const Cursor& LEGACY_NODELETE verticalTextCursor();
const Cursor& LEGACY_NODELETE cellCursor();
const Cursor& LEGACY_NODELETE contextMenuCursor();
const Cursor& LEGACY_NODELETE noDropCursor();
const Cursor& LEGACY_NODELETE notAllowedCursor();
const Cursor& LEGACY_NODELETE progressCursor();
const Cursor& LEGACY_NODELETE aliasCursor();
const Cursor& LEGACY_NODELETE zoomInCursor();
const Cursor& LEGACY_NODELETE zoomOutCursor();
const Cursor& LEGACY_NODELETE copyCursor();
const Cursor& LEGACY_NODELETE noneCursor();
const Cursor& LEGACY_NODELETE grabCursor();
const Cursor& LEGACY_NODELETE grabbingCursor();

inline Cursor::Type Cursor::type() const
{
    ASSERT(m_type > Type::Invalid);
    ASSERT(m_type <= Type::Custom);
    return m_type;
}

} // namespace WebCore
