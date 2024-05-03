/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PixelFormatValidated.h"

#include "PixelFormat.h"

namespace WebCore {

PixelFormatValidated convertPixelFormatToPixelFormatValidated(PixelFormat format)
{
    switch (format) {
    case PixelFormat::RGBA8:
        ASSERT_NOT_REACHED();
        return PixelFormatValidated::BGRX8;
    case PixelFormat::BGRX8:
        return PixelFormatValidated::BGRX8;
    case PixelFormat::BGRA8:
        return PixelFormatValidated::BGRA8;
#if HAVE(IOSURFACE_RGB10)
    case PixelFormat::RGB10:
        return PixelFormatValidated::RGB10;
    case PixelFormat::RGB10A8:
        return PixelFormatValidated::RGB10A8;
#else
    case PixelFormat::RGB10:
    case PixelFormat::RGB10A8:
        ASSERT_NOT_REACHED();
        return PixelFormatValidated::BGRX8;
#endif
    }

    ASSERT_NOT_REACHED();
    return PixelFormatValidated::BGRX8;
}

PixelFormat convertPixelFormatValidatedToPixelFormat(PixelFormatValidated format)
{
    switch (format) {
    case PixelFormatValidated::BGRX8:
        return PixelFormat::BGRX8;
    case PixelFormatValidated::BGRA8:
        return PixelFormat::BGRA8;
#if HAVE(IOSURFACE_RGB10)
    case PixelFormatValidated::RGB10:
        return PixelFormat::RGB10;
    case PixelFormatValidated::RGB10A8:
        return PixelFormat::RGB10A8;
#endif
    }

    ASSERT_NOT_REACHED();
    return PixelFormat::BGRX8;
}

}
