/*
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "Cursor.h"

#include "BitmapInfo.h"
#include "Image.h"
#include "IntPoint.h"

#include <wtf/OwnPtr.h>

#include <windows.h>

#define ALPHA_CURSORS

namespace WebCore {

Cursor::Cursor(const Cursor& other)
    : m_impl(other.m_impl)
{
}

static inline bool supportsAlphaCursors() 
{
    OSVERSIONINFO osinfo = {0};
    osinfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&osinfo);
    return osinfo.dwMajorVersion > 5 || (osinfo.dwMajorVersion == 5 && osinfo.dwMinorVersion > 0);
}

Cursor::Cursor(Image* img, const IntPoint& hotspot) 
{ 
    static bool doAlpha = supportsAlphaCursors();
    BitmapInfo cursorImage = BitmapInfo::create(IntSize(img->width(), img->height()));

    HDC dc = GetDC(0);
    HDC workingDC = CreateCompatibleDC(dc);
    if (doAlpha) {
        OwnPtr<HBITMAP> hCursor(CreateDIBSection(dc, (BITMAPINFO *)&cursorImage, DIB_RGB_COLORS, 0, 0, 0));
        ASSERT(hCursor);

        img->getHBITMAP(hCursor.get()); 
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(workingDC, hCursor.get());
        SetBkMode(workingDC, TRANSPARENT);
        SelectObject(workingDC, hOldBitmap);

        Vector<unsigned char, 128> maskBits;
        maskBits.fill(0xff, (img->width() + 7) / 8 * img->height());
        OwnPtr<HBITMAP> hMask(CreateBitmap(img->width(), img->height(), 1, 1, maskBits.data()));

        ICONINFO ii;
        ii.fIcon = FALSE;
        ii.xHotspot = hotspot.x();
        ii.yHotspot = hotspot.y();
        ii.hbmMask = hMask.get();
        ii.hbmColor = hCursor.get();

        m_impl = SharedCursor::create(CreateIconIndirect(&ii));
    } else {
        // Platform doesn't support alpha blended cursors, so we need
        // to create the mask manually
        HDC andMaskDC = CreateCompatibleDC(dc);
        HDC xorMaskDC = CreateCompatibleDC(dc);
        OwnPtr<HBITMAP> hCursor(CreateDIBSection(dc, &cursorImage, DIB_RGB_COLORS, 0, 0, 0));
        ASSERT(hCursor);
        img->getHBITMAP(hCursor.get()); 
        BITMAP cursor;
        GetObject(hCursor.get(), sizeof(BITMAP), &cursor);
        OwnPtr<HBITMAP> andMask(CreateBitmap(cursor.bmWidth, cursor.bmHeight, 1, 1, NULL));
        OwnPtr<HBITMAP> xorMask(CreateCompatibleBitmap(dc, cursor.bmWidth, cursor.bmHeight));
        HBITMAP oldCursor = (HBITMAP)SelectObject(workingDC, hCursor.get());
        HBITMAP oldAndMask = (HBITMAP)SelectObject(andMaskDC, andMask.get());
        HBITMAP oldXorMask = (HBITMAP)SelectObject(xorMaskDC, xorMask.get());

        SetBkColor(workingDC, RGB(0,0,0));  
        BitBlt(andMaskDC, 0, 0, cursor.bmWidth, cursor.bmHeight, workingDC, 0, 0, SRCCOPY);
    
        SetBkColor(xorMaskDC, RGB(255, 255, 255));
        SetTextColor(xorMaskDC, RGB(255, 255, 255));
        BitBlt(xorMaskDC, 0, 0, cursor.bmWidth, cursor.bmHeight, andMaskDC, 0, 0, SRCCOPY);
        BitBlt(xorMaskDC, 0, 0, cursor.bmWidth, cursor.bmHeight, workingDC, 0,0, SRCAND);

        SelectObject(workingDC, oldCursor);
        SelectObject(andMaskDC, oldAndMask);
        SelectObject(xorMaskDC, oldXorMask);

        ICONINFO icon = {0};
        icon.fIcon = FALSE;
        icon.xHotspot = hotspot.x();
        icon.yHotspot = hotspot.y();
        icon.hbmMask = andMask.get();
        icon.hbmColor = xorMask.get();
        m_impl = SharedCursor::create(CreateIconIndirect(&icon));

        DeleteDC(xorMaskDC);
        DeleteDC(andMaskDC);
    }
    DeleteDC(workingDC);
    ReleaseDC(0, dc);
}

Cursor::~Cursor()
{
}

Cursor& Cursor::operator=(const Cursor& other)
{
    m_impl = other.m_impl;
    return *this;
}

Cursor::Cursor(PlatformCursor c)
    : m_impl(c)
{
}

static Cursor loadCursorByName(char* name, int x, int y) 
{
    IntPoint hotSpot(x, y);
    Cursor c;
    RefPtr<Image> cursorImage(Image::loadPlatformResource(name));
    if (cursorImage && !cursorImage->isNull()) 
        c = Cursor(cursorImage.get(), hotSpot);
    else
        c = pointerCursor();
    return c;
}

static PassRefPtr<SharedCursor> loadSharedCursor(HINSTANCE hInstance, LPCTSTR lpCursorName)
{
    return SharedCursor::create(LoadCursor(hInstance, lpCursorName));
}

const Cursor& pointerCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_ARROW);
    return c;
}

const Cursor& crossCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_CROSS);
    return c;
}

const Cursor& handCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_HAND);
    return c;
}

const Cursor& iBeamCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_IBEAM);
    return c;
}

const Cursor& waitCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_WAIT);
    return c;
}

const Cursor& helpCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_HELP);
    return c;
}

const Cursor& eastResizeCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_SIZEWE);
    return c;
}

const Cursor& northResizeCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_SIZENS);
    return c;
}

const Cursor& northEastResizeCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_SIZENESW);
    return c;
}

const Cursor& northWestResizeCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_SIZENWSE);
    return c;
}

const Cursor& southResizeCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_SIZENS);
    return c;
}

const Cursor& southEastResizeCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_SIZENWSE);
    return c;
}

const Cursor& southWestResizeCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_SIZENESW);
    return c;
}

const Cursor& westResizeCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_SIZEWE);
    return c;
}

const Cursor& northSouthResizeCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_SIZENS);
    return c;
}

const Cursor& eastWestResizeCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_SIZEWE);
    return c;
}

const Cursor& northEastSouthWestResizeCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_SIZENESW);
    return c;
}

const Cursor& northWestSouthEastResizeCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_SIZENWSE);
    return c;
}

const Cursor& columnResizeCursor()
{
    // FIXME: Windows does not have a standard column resize cursor <rdar://problem/5018591>
    static Cursor c = loadSharedCursor(0, IDC_SIZEWE);
    return c;
}

const Cursor& rowResizeCursor()
{
    // FIXME: Windows does not have a standard row resize cursor <rdar://problem/5018591>
    static Cursor c = loadSharedCursor(0, IDC_SIZENS);
    return c;
}

const Cursor& middlePanningCursor()
{
    static const Cursor c = loadCursorByName("panIcon", 7, 7);
    return c;
}

const Cursor& eastPanningCursor()
{
    static const Cursor c = loadCursorByName("panEastCursor", 7, 7);
    return c;
}

const Cursor& northPanningCursor()
{
    static const Cursor c = loadCursorByName("panNorthCursor", 7, 7);
    return c;
}

const Cursor& northEastPanningCursor()
{
    static const Cursor c = loadCursorByName("panNorthEastCursor", 7, 7);
    return c;
}

const Cursor& northWestPanningCursor()
{
    static const Cursor c = loadCursorByName("panNorthWestCursor", 7, 7);
    return c;
}

const Cursor& southPanningCursor()
{
    static const Cursor c = loadCursorByName("panSouthCursor", 7, 7);
    return c;
}

const Cursor& southEastPanningCursor()
{
    static const Cursor c = loadCursorByName("panSouthEastCursor", 7, 7);
    return c;
}

const Cursor& southWestPanningCursor()
{
    static const Cursor c = loadCursorByName("panSouthWestCursor", 7, 7);
    return c;
}

const Cursor& westPanningCursor()
{
    static const Cursor c = loadCursorByName("panWestCursor", 7, 7);
    return c;
}

const Cursor& moveCursor() 
{
    static Cursor c = loadSharedCursor(0, IDC_SIZEALL);
    return c;
}

const Cursor& verticalTextCursor()
{
    static const Cursor c = loadCursorByName("verticalTextCursor", 7, 7);
    return c;
}

const Cursor& cellCursor()
{
    return pointerCursor();
}

const Cursor& contextMenuCursor()
{
    return pointerCursor();
}

const Cursor& aliasCursor()
{
    return pointerCursor();
}

const Cursor& progressCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_APPSTARTING);
    return c;
}

const Cursor& noDropCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_NO);
    return c;
}

const Cursor& copyCursor()
{
    return pointerCursor();
}

const Cursor& noneCursor()
{
    return pointerCursor();
}

const Cursor& notAllowedCursor()
{
    static Cursor c = loadSharedCursor(0, IDC_NO);
    return c;
}

const Cursor& zoomInCursor()
{
    static const Cursor c = loadCursorByName("zoomInCursor", 7, 7);
    return c;
}

const Cursor& zoomOutCursor()
{
    static const Cursor c = loadCursorByName("zoomOutCursor", 7, 7);
    return c;
}

const Cursor& grabCursor()
{
    return pointerCursor();
}

const Cursor& grabbingCursor()
{
    return pointerCursor();
}

}
