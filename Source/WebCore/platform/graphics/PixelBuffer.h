/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include "ColorSpace.h"
#include "IntSize.h"
#include "PixelFormat.h"
#include <JavaScriptCore/Uint8ClampedArray.h>

namespace WebCore {

class PixelBuffer {
    WTF_MAKE_NONCOPYABLE(PixelBuffer);
public:
    PixelBuffer(DestinationColorSpace, PixelFormat, const IntSize&, Ref<JSC::Uint8ClampedArray>&&);
    ~PixelBuffer();

    PixelBuffer(PixelBuffer&&) = default;
    PixelBuffer& operator=(PixelBuffer&&) = default;

    DestinationColorSpace colorSpace() const { return m_colorSpace; }
    PixelFormat format() const { return m_format; }
    const IntSize& size() const { return m_size; }
    JSC::Uint8ClampedArray& data() const { return m_data.get(); }

    PixelBuffer deepClone() const;

private:
    DestinationColorSpace m_colorSpace;
    PixelFormat m_format;
    IntSize m_size;
    Ref<JSC::Uint8ClampedArray> m_data;
};

}
