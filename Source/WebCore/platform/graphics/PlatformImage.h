/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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

#if USE(CG)
#include <wtf/RetainPtr.h>
typedef struct CGImage* CGImageRef;
#elif USE(CAIRO)
#include "RefPtrCairo.h"
#elif USE(WINGDI)
#include "SharedBitmap.h"
#elif USE(HAIKU)
#include <Bitmap.h>
#endif

namespace WebCore {

#if USE(CG)
using PlatformImagePtr = RetainPtr<CGImageRef>;
#elif USE(DIRECT2D)
using PlatformImagePtr = COMPtr<ID2D1Bitmap>;
#elif USE(CAIRO)
using PlatformImagePtr = RefPtr<cairo_surface_t>;
#elif USE(WINGDI)
using PlatformImagePtr = RefPtr<SharedBitmap>;
#elif USE(HAIKU)
class BitmapRef: public BBitmap, public RefCounted<BitmapRef>
{
    public:
        BitmapRef(BRect r, uint32 f, color_space c, int32 b)
            : BBitmap(r, f, c, b)
        {
        }

        BitmapRef(BRect r, color_space c, bool v)
            : BBitmap(r, c, v)
        {
        }

        BitmapRef(const BBitmap& other)
            : BBitmap(other)
        {
        }

        BitmapRef(const BitmapRef& other) = delete;

        ~BitmapRef()
        {
        }
};

using PlatformImagePtr = RefPtr<BitmapRef>;
#endif

}
