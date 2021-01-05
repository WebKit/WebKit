/*
 * Copyright (C) 2013 Haiku, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PixelDumpSupportHaiku_h
#define PixelDumpSupportHaiku_h

#include <interface/Bitmap.h>
#include <Size.h>

#include <stdio.h>
#include <wtf/RefPtr.h>
#include <wtf/RefCounted.h>


class BitmapContext : public RefCounted<BitmapContext> {
public:

    static RefPtr<BitmapContext> createByAdoptingData(BSize size,
        BBitmap* bitmap)
    {
		BitmapContext* context = new BitmapContext(size, bitmap);
        return adoptRef(*context);
    }

    ~BitmapContext()
    {
        delete m_bitmap;
    }

    BBitmap* m_bitmap;

private:

    BitmapContext(BSize size, BBitmap* bitmap)
    {
        // The size may be smaller than the bitmap, as we only want to keep the
        // viewport.
        m_bitmap = new BBitmap(BRect(0, 0, size.Width() - 1, size.Height() - 1), 0,
            B_RGBA32);
        m_bitmap->ImportBits(bitmap, BPoint(0, 0), BPoint(0, 0), size.Width(),
            size.Height());
    }
};

#endif // PixelDumpSupportHaiku_h
