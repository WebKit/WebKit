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

#ifndef CCTexture_h
#define CCTexture_h

#include "CCResourceProvider.h"
#include "CCTexture.h"
#include "GraphicsContext3D.h"
#include "IntSize.h"

namespace WebCore {

class CCTexture {
public:
    CCTexture() : m_id(0) { }
    CCTexture(unsigned id, IntSize size, GC3Denum format)
        : m_id(id)
        , m_size(size)
        , m_format(format) { }

    CCResourceProvider::ResourceId id() const { return m_id; }
    const IntSize& size() const { return m_size; }
    GC3Denum format() const { return m_format; }

    void setId(CCResourceProvider::ResourceId id) { m_id = id; }
    void setDimensions(const IntSize&, GC3Denum format);

    size_t bytes() const;

    static size_t memorySizeBytes(const IntSize&, GC3Denum format);

private:
    CCResourceProvider::ResourceId m_id;
    IntSize m_size;
    GC3Denum m_format;
};

}

#endif
