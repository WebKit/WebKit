/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef PixelDumpSupportBlackBerry_h
#define PixelDumpSupportBlackBerry_h

#include <skia/SkBitmap.h>
#include <skia/SkCanvas.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

class BitmapContext : public RefCounted<BitmapContext> {
public:

    static PassRefPtr<BitmapContext> createByAdoptingBitmapAndContext(SkBitmap* bitmap, SkCanvas* canvas)
    {
        return adoptRef(new BitmapContext(bitmap, canvas));
    }

    ~BitmapContext()
    {
        delete m_bitmap;
        delete m_canvas;
    }

    SkCanvas* canvas() { return m_canvas; }

private:

    BitmapContext(SkBitmap* bitmap, SkCanvas* canvas)
        : m_bitmap(bitmap)
        , m_canvas(canvas)
    {
    }

    SkBitmap* m_bitmap;
    SkCanvas* m_canvas;

};

#endif // PixelDumpSupportBlackBerry_h
