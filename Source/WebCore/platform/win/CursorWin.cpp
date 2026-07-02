/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Cursor.h"

#include "BitmapInfo.h"
#include "HWndDC.h"
#include "Image.h"
#include "ImageAdapter.h"
#include "IntPoint.h"
#include <wtf/win/GDIObject.h>

#include <windows.h>

#define ALPHA_CURSORS

namespace WebCore {

SharedCursor::SharedCursor(HCURSOR nativeCursor)
    : m_nativeCursor(nativeCursor)
{
}

Ref<SharedCursor> SharedCursor::create(HCURSOR nativeCursor)
{
    return adoptRef(*new SharedCursor(nativeCursor));
}

SharedCursor::~SharedCursor()
{
    DestroyIcon(m_nativeCursor);
}

static Ref<SharedCursor> createSharedCursor(Image* img, const IntPoint& hotSpot)
{
    IntPoint effectiveHotSpot = determineHotSpot(img, hotSpot);
    BitmapInfo cursorImage = BitmapInfo::create(IntSize(img->width(), img->height()));

    HWndDC dc(0);
    auto workingDC = adoptGDIObject(::CreateCompatibleDC(dc));
    auto hCursor = adoptGDIObject(::CreateDIBSection(dc, &cursorImage, DIB_RGB_COLORS, nullptr, 0, 0));

    img->adapter().getHBITMAP(hCursor.get());
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(workingDC.get(), hCursor.get());
    SetBkMode(workingDC.get(), TRANSPARENT);
    SelectObject(workingDC.get(), hOldBitmap);

    Vector<unsigned char, 128> maskBits;
    maskBits.fill(0xff, (img->width() + 7) / 8 * img->height());
    auto hMask = adoptGDIObject(::CreateBitmap(img->width(), img->height(), 1, 1, maskBits.span().data()));

    ICONINFO ii;
    ii.fIcon = FALSE;
    ii.xHotspot = effectiveHotSpot.x();
    ii.yHotspot = effectiveHotSpot.y();
    ii.hbmMask = hMask.get();
    ii.hbmColor = hCursor.get();

    return SharedCursor::create(::CreateIconIndirect(&ii));
}

static Ref<SharedCursor> loadSharedCursor(HINSTANCE hInstance, LPCWSTR lpCursorName)
{
    return SharedCursor::create(::LoadCursorW(hInstance, lpCursorName));
}

static Ref<SharedCursor> loadCursorByName(const char* name, int x, int y)
{
    IntPoint hotSpot(x, y);
    RefPtr<Image> cursorImage(ImageAdapter::loadPlatformResource(name));
    if (cursorImage && !cursorImage->isNull())
        return createSharedCursor(cursorImage.get(), hotSpot);
    return loadSharedCursor(0, IDC_ARROW);
}

void Cursor::ensurePlatformCursor() const
{
    if (m_platformCursor)
        return;

    switch (m_type) {
    case Type::Pointer:
    case Type::Cell:
    case Type::ContextMenu:
    case Type::Alias:
    case Type::Copy:
    case Type::None:
    case Type::Grab:
    case Type::Grabbing:
        m_platformCursor = loadSharedCursor(0, IDC_ARROW);
        break;
    case Type::Cross:
        m_platformCursor = loadSharedCursor(0, IDC_CROSS);
        break;
    case Type::Hand:
        m_platformCursor = loadSharedCursor(0, IDC_HAND);
        break;
    case Type::IBeam:
        m_platformCursor = loadSharedCursor(0, IDC_IBEAM);
        break;
    case Type::Wait:
        m_platformCursor = loadSharedCursor(0, IDC_WAIT);
        break;
    case Type::Help:
        m_platformCursor = loadSharedCursor(0, IDC_HELP);
        break;
    case Type::Move:
        m_platformCursor = loadSharedCursor(0, IDC_SIZEALL);
        break;
    case Type::MiddlePanning:
        m_platformCursor = loadCursorByName("panIcon", 8, 8);
        break;
    case Type::EastResize:
        m_platformCursor = loadSharedCursor(0, IDC_SIZEWE);
        break;
    case Type::EastPanning:
        m_platformCursor = loadCursorByName("panEastCursor", 7, 7);
        break;
    case Type::NorthResize:
        m_platformCursor = loadSharedCursor(0, IDC_SIZENS);
        break;
    case Type::NorthPanning:
        m_platformCursor = loadCursorByName("panNorthCursor", 7, 7);
        break;
    case Type::NorthEastResize:
        m_platformCursor = loadSharedCursor(0, IDC_SIZENESW);
        break;
    case Type::NorthEastPanning:
        m_platformCursor = loadCursorByName("panNorthEastCursor", 7, 7);
        break;
    case Type::NorthWestResize:
        m_platformCursor = loadSharedCursor(0, IDC_SIZENWSE);
        break;
    case Type::NorthWestPanning:
        m_platformCursor = loadCursorByName("panNorthWestCursor", 7, 7);
        break;
    case Type::SouthResize:
        m_platformCursor = loadSharedCursor(0, IDC_SIZENS);
        break;
    case Type::SouthPanning:
        m_platformCursor = loadCursorByName("panSouthCursor", 7, 7);
        break;
    case Type::SouthEastResize:
        m_platformCursor = loadSharedCursor(0, IDC_SIZENWSE);
        break;
    case Type::SouthEastPanning:
        m_platformCursor = loadCursorByName("panSouthEastCursor", 7, 7);
        break;
    case Type::SouthWestResize:
        m_platformCursor = loadSharedCursor(0, IDC_SIZENESW);
        break;
    case Type::SouthWestPanning:
        m_platformCursor = loadCursorByName("panSouthWestCursor", 7, 7);
        break;
    case Type::WestResize:
        m_platformCursor = loadSharedCursor(0, IDC_SIZEWE);
        break;
    case Type::NorthSouthResize:
        m_platformCursor = loadSharedCursor(0, IDC_SIZENS);
        break;
    case Type::EastWestResize:
        m_platformCursor = loadSharedCursor(0, IDC_SIZEWE);
        break;
    case Type::WestPanning:
        m_platformCursor = loadCursorByName("panWestCursor", 7, 7);
        break;
    case Type::NorthEastSouthWestResize:
        m_platformCursor = loadSharedCursor(0, IDC_SIZENESW);
        break;
    case Type::NorthWestSouthEastResize:
        m_platformCursor = loadSharedCursor(0, IDC_SIZENWSE);
        break;
    case Type::ColumnResize:
        // FIXME: Windows does not have a standard column resize cursor <rdar://problem/5018591>
        m_platformCursor = loadSharedCursor(0, IDC_SIZEWE);
        break;
    case Type::RowResize:
        // FIXME: Windows does not have a standard row resize cursor <rdar://problem/5018591>
        m_platformCursor = loadSharedCursor(0, IDC_SIZENS);
        break;
    case Type::VerticalText:
        m_platformCursor = loadCursorByName("verticalTextCursor", 7, 7);
        break;
    case Type::Progress:
        m_platformCursor = loadSharedCursor(0, IDC_APPSTARTING);
        break;
    case Type::NoDrop:
    case Type::NotAllowed:
        m_platformCursor = loadSharedCursor(0, IDC_NO);
        break;
    case Type::ZoomIn:
        m_platformCursor = loadCursorByName("zoomInCursor", 7, 7);
        break;
    case Type::ZoomOut:
        m_platformCursor = loadCursorByName("zoomOutCursor", 7, 7);
        break;
    case Type::Custom:
        if (m_image->isNull())
            m_platformCursor = loadSharedCursor(0, IDC_ARROW);
        else
            m_platformCursor = createSharedCursor(m_image.get(), m_hotSpot);
        break;
    case Type::Invalid:
    default:
        ASSERT_NOT_REACHED();
        m_platformCursor = loadSharedCursor(0, IDC_ARROW);
        break;
    }
}

} // namespace WebCore
