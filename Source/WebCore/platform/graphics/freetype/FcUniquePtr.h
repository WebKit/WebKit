/*
 * Copyright (C) 2015 Igalia S.L
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

#ifndef FcUniquePtr_h
#define FcUniquePtr_h

#if USE(FREETYPE)

#include <fontconfig/fontconfig.h>
#include <memory>

namespace WebCore {

template<typename T>
struct FcPtrDeleter {
    void operator()(T* ptr) const = delete;
};

template<typename T>
using FcUniquePtr = std::unique_ptr<T, FcPtrDeleter<T>>;

template<> struct FcPtrDeleter<FcCharSet> {
    void operator()(FcCharSet* ptr) const
    {
        FcCharSetDestroy(ptr);
    }
};

template<> struct FcPtrDeleter<FcFontSet> {
    void operator()(FcFontSet* ptr) const
    {
        FcFontSetDestroy(ptr);
    }
};

template<> struct FcPtrDeleter<FcLangSet> {
    void operator()(FcLangSet* ptr) const
    {
        FcLangSetDestroy(ptr);
    }
};

template<> struct FcPtrDeleter<FcObjectSet> {
    void operator()(FcObjectSet* ptr) const
    {
        FcObjectSetDestroy(ptr);
    }
};

} // namespace WebCore

#endif // USE(FREETYPE)

#endif // FcUniquePtr_h
