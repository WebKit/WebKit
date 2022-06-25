/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_RTC)

#include <wtf/Vector.h>

namespace WebCore {

struct SFrameCompatibilityPrefixBuffer {
    const uint8_t* data { nullptr };
    size_t size { 0 };
    Vector<uint8_t> buffer;
};

size_t computeH264PrefixOffset(const uint8_t*, size_t);
SFrameCompatibilityPrefixBuffer computeH264PrefixBuffer(const uint8_t*, size_t);

WEBCORE_EXPORT bool needsRbspUnescaping(const uint8_t*, size_t);
WEBCORE_EXPORT Vector<uint8_t> fromRbsp(const uint8_t*, size_t);
WEBCORE_EXPORT void toRbsp(Vector<uint8_t>&, size_t);

size_t computeVP8PrefixOffset(const uint8_t*, size_t);
SFrameCompatibilityPrefixBuffer computeVP8PrefixBuffer(const uint8_t*, size_t);

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
