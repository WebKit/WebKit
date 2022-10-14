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

#include "Image.h"
#include "IntPoint.h"
#include <wtf/Assertions.h>
#include <wtf/EnumTraits.h>
#include <wtf/RefPtr.h>

#if PLATFORM(WIN)
typedef struct HICON__* HICON;
typedef HICON HCURSOR;
#include <wtf/RefCounted.h>
#elif PLATFORM(COCOA)
#include <wtf/RetainPtr.h>
#elif PLATFORM(GTK)
#include "GRefPtrGtk.h"
#endif

#if HAVE(NSCURSOR)
OBJC_CLASS NSCursor;
#endif

#if PLATFORM(WIN)
typedef struct HICON__ *HICON;
typedef HICON HCURSOR;
#endif

namespace WebCore {

class Image;

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
#elif PLATFORM(GTK)
using PlatformCursor = GRefPtr<GdkCursor>;
#else
using PlatformCursor = void*;
#endif

class Cursor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum Type {
        Pointer = 0,
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

    Cursor() = default;

    WEBCORE_EXPORT static const Cursor& fromType(Cursor::Type);

    WEBCORE_EXPORT Cursor(Image*, const IntPoint& hotSpot);

#if ENABLE(MOUSE_CURSOR_SCALE)
    // Hot spot is in image pixels.
    WEBCORE_EXPORT Cursor(Image*, const IntPoint& hotSpot, float imageScaleFactor);
#endif

    explicit Cursor(Type);

    Type type() const;
    RefPtr<Image> image() const { return m_image; }
    const IntPoint& hotSpot() const { return m_hotSpot; }

#if ENABLE(MOUSE_CURSOR_SCALE)
    // Image scale in image pixels per logical (UI) pixel.
    float imageScaleFactor() const { return m_imageScaleFactor; }
#endif

    WEBCORE_EXPORT PlatformCursor platformCursor() const;

    WEBCORE_EXPORT void setAsPlatformCursor() const;

private:
    void ensurePlatformCursor() const;

    // The type of -1 indicates an invalid Cursor that should never actually get used.
    Type m_type { static_cast<Type>(-1) };
    RefPtr<Image> m_image;
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

IntPoint determineHotSpot(Image*, const IntPoint& specifiedHotSpot);

WEBCORE_EXPORT const Cursor& pointerCursor();
const Cursor& crossCursor();
WEBCORE_EXPORT const Cursor& handCursor();
const Cursor& moveCursor();
WEBCORE_EXPORT const Cursor& iBeamCursor();
const Cursor& waitCursor();
const Cursor& helpCursor();
const Cursor& eastResizeCursor();
const Cursor& northResizeCursor();
const Cursor& northEastResizeCursor();
const Cursor& northWestResizeCursor();
const Cursor& southResizeCursor();
const Cursor& southEastResizeCursor();
const Cursor& southWestResizeCursor();
const Cursor& westResizeCursor();
const Cursor& northSouthResizeCursor();
const Cursor& eastWestResizeCursor();
const Cursor& northEastSouthWestResizeCursor();
const Cursor& northWestSouthEastResizeCursor();
const Cursor& columnResizeCursor();
const Cursor& rowResizeCursor();
const Cursor& middlePanningCursor();
const Cursor& eastPanningCursor();
const Cursor& northPanningCursor();
const Cursor& northEastPanningCursor();
const Cursor& northWestPanningCursor();
const Cursor& southPanningCursor();
const Cursor& southEastPanningCursor();
const Cursor& southWestPanningCursor();
const Cursor& westPanningCursor();
const Cursor& verticalTextCursor();
const Cursor& cellCursor();
const Cursor& contextMenuCursor();
const Cursor& noDropCursor();
const Cursor& notAllowedCursor();
const Cursor& progressCursor();
const Cursor& aliasCursor();
const Cursor& zoomInCursor();
const Cursor& zoomOutCursor();
const Cursor& copyCursor();
const Cursor& noneCursor();
const Cursor& grabCursor();
const Cursor& grabbingCursor();

inline Cursor::Type Cursor::type() const
{
    ASSERT(m_type >= 0);
    ASSERT(m_type <= Custom);
    return m_type;
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::Cursor::Type> {
    using values = EnumValues<
        WebCore::Cursor::Type,
        WebCore::Cursor::Type::Pointer,
        WebCore::Cursor::Type::Cross,
        WebCore::Cursor::Type::Hand,
        WebCore::Cursor::Type::IBeam,
        WebCore::Cursor::Type::Wait,
        WebCore::Cursor::Type::Help,
        WebCore::Cursor::Type::EastResize,
        WebCore::Cursor::Type::NorthResize,
        WebCore::Cursor::Type::NorthEastResize,
        WebCore::Cursor::Type::NorthWestResize,
        WebCore::Cursor::Type::SouthResize,
        WebCore::Cursor::Type::SouthEastResize,
        WebCore::Cursor::Type::SouthWestResize,
        WebCore::Cursor::Type::WestResize,
        WebCore::Cursor::Type::NorthSouthResize,
        WebCore::Cursor::Type::EastWestResize,
        WebCore::Cursor::Type::NorthEastSouthWestResize,
        WebCore::Cursor::Type::NorthWestSouthEastResize,
        WebCore::Cursor::Type::ColumnResize,
        WebCore::Cursor::Type::RowResize,
        WebCore::Cursor::Type::MiddlePanning,
        WebCore::Cursor::Type::EastPanning,
        WebCore::Cursor::Type::NorthPanning,
        WebCore::Cursor::Type::NorthEastPanning,
        WebCore::Cursor::Type::NorthWestPanning,
        WebCore::Cursor::Type::SouthPanning,
        WebCore::Cursor::Type::SouthEastPanning,
        WebCore::Cursor::Type::SouthWestPanning,
        WebCore::Cursor::Type::WestPanning,
        WebCore::Cursor::Type::Move,
        WebCore::Cursor::Type::VerticalText,
        WebCore::Cursor::Type::Cell,
        WebCore::Cursor::Type::ContextMenu,
        WebCore::Cursor::Type::Alias,
        WebCore::Cursor::Type::Progress,
        WebCore::Cursor::Type::NoDrop,
        WebCore::Cursor::Type::Copy,
        WebCore::Cursor::Type::None,
        WebCore::Cursor::Type::NotAllowed,
        WebCore::Cursor::Type::ZoomIn,
        WebCore::Cursor::Type::ZoomOut,
        WebCore::Cursor::Type::Grab,
        WebCore::Cursor::Type::Grabbing,
        WebCore::Cursor::Type::Custom
    >;
};

} // namespace WTF
