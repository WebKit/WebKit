/*
 * Copyright (C) 2017 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(HARFBUZZ)

#include <hb.h>

namespace WebCore {

template<typename T>
struct HbPtrDeleter {
    void operator()(T* ptr) const = delete;
};

template<typename T>
using HbUniquePtr = std::unique_ptr<T, HbPtrDeleter<T>>;

template<> struct HbPtrDeleter<hb_font_t> {
    void operator()(hb_font_t* ptr) const
    {
        hb_font_destroy(ptr);
    }
};

template<> struct HbPtrDeleter<hb_buffer_t> {
    void operator()(hb_buffer_t* ptr) const
    {
        hb_buffer_destroy(ptr);
    }
};

template<> struct HbPtrDeleter<hb_face_t> {
    void operator()(hb_face_t* ptr) const
    {
        hb_face_destroy(ptr);
    }
};

} // namespace WebCore

using WebCore::HbUniquePtr;

#endif // USE(HARFBUZZ)
