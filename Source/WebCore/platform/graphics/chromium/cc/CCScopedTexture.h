/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef CCScopedTexture_h
#define CCScopedTexture_h

#include "TextureAllocator.h"
#include "cc/CCTexture.h"

#if !ASSERT_DISABLED
#include <wtf/MainThread.h>
#endif

namespace WebCore {


class CCScopedTexture : protected CCTexture {
    WTF_MAKE_NONCOPYABLE(CCScopedTexture);
public:
    static PassOwnPtr<CCScopedTexture> create(TextureAllocator* allocator) { return adoptPtr(new CCScopedTexture(allocator)); }
    virtual ~CCScopedTexture();

    using CCTexture::id;
    using CCTexture::size;
    using CCTexture::format;
    using CCTexture::bytes;

    bool allocate(const IntSize&, GC3Denum format);
    void free();
    void leak();

protected:
    explicit CCScopedTexture(TextureAllocator*);

private:
    TextureAllocator* m_allocator;

#if !ASSERT_DISABLED
    ThreadIdentifier m_allocateThreadIdentifier;
#endif
};

}

#endif
