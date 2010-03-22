/*
 * Copyright (C) 2004, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef Cursor_h
#define Cursor_h

#if PLATFORM(WIN)
typedef struct HICON__* HICON;
typedef HICON HCURSOR;
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#elif PLATFORM(GTK)
typedef struct _GdkCursor GdkCursor;
#elif PLATFORM(QT)
#include <QCursor>
#elif PLATFORM(CHROMIUM)
#include "PlatformCursor.h"
#elif PLATFORM(HAIKU)
#include <app/Cursor.h>
#endif

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSCursor;
#else
class NSCursor;
#endif
#endif

#if PLATFORM(WX)
class wxCursor;
#endif

#if PLATFORM(WIN)
typedef struct HICON__ *HICON;
typedef HICON HCURSOR;
#endif

namespace WebCore {

    class Image;
    class IntPoint;

#if PLATFORM(WIN)
    class SharedCursor : public RefCounted<SharedCursor> {
    public:
        static PassRefPtr<SharedCursor> create(HCURSOR nativeCursor) { return adoptRef(new SharedCursor(nativeCursor)); }
        ~SharedCursor();
        HCURSOR nativeCursor() const { return m_nativeCursor; }
    private:
        SharedCursor(HCURSOR nativeCursor) : m_nativeCursor(nativeCursor) { }
        HCURSOR m_nativeCursor;
    };
    typedef RefPtr<SharedCursor> PlatformCursor;
    typedef HCURSOR PlatformCursorHandle;
#elif PLATFORM(MAC)
    typedef NSCursor* PlatformCursor;
    typedef NSCursor* PlatformCursorHandle;
#elif PLATFORM(GTK)
    typedef GdkCursor* PlatformCursor;
    typedef GdkCursor* PlatformCursorHandle;
#elif PLATFORM(EFL)
    typedef const char* PlatformCursor;
    typedef const char* PlatformCursorHandle;
#elif PLATFORM(QT) && !defined(QT_NO_CURSOR)
    typedef QCursor PlatformCursor;
    typedef QCursor* PlatformCursorHandle;
#elif PLATFORM(WX)
    typedef wxCursor* PlatformCursor;
    typedef wxCursor* PlatformCursorHandle;
#elif PLATFORM(CHROMIUM)
    // See PlatformCursor.h
    typedef void* PlatformCursorHandle;
#elif PLATFORM(HAIKU)
    typedef BCursor* PlatformCursor;
    typedef BCursor* PlatformCursorHandle;
#else
    typedef void* PlatformCursor;
    typedef void* PlatformCursorHandle;
#endif

    class Cursor {
    public:
        Cursor()
#if !PLATFORM(QT) && !PLATFORM(EFL)
        : m_impl(0)
#endif
        { }

        Cursor(Image*, const IntPoint& hotspot);
        Cursor(const Cursor&);
        ~Cursor();
        Cursor& operator=(const Cursor&);

        Cursor(PlatformCursor);
        PlatformCursor impl() const { return m_impl; }

     private:
        PlatformCursor m_impl;
    };

    const Cursor& pointerCursor();
    const Cursor& crossCursor();
    const Cursor& handCursor();
    const Cursor& moveCursor();
    const Cursor& iBeamCursor();
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

} // namespace WebCore

#endif // Cursor_h
