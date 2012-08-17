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

#include "config.h"

#include "CCScopedTexture.h"

namespace WebCore {

CCScopedTexture::CCScopedTexture(CCResourceProvider* resourceProvider)
    : m_resourceProvider(resourceProvider)
{
    ASSERT(m_resourceProvider);
}

CCScopedTexture::~CCScopedTexture()
{
    free();
}

bool CCScopedTexture::allocate(int pool, const IntSize& size, GC3Denum format, CCResourceProvider::TextureUsageHint hint)
{
    ASSERT(!id());
    ASSERT(!size.isEmpty());

    setDimensions(size, format);
    setId(m_resourceProvider->createResource(pool, size, format, hint));

#if !ASSERT_DISABLED
    m_allocateThreadIdentifier = WTF::currentThread();
#endif

    return id();
}

void CCScopedTexture::free()
{
    if (id()) {
        ASSERT(m_allocateThreadIdentifier == WTF::currentThread());
        m_resourceProvider->deleteResource(id());
    }
    setId(0);
}

void CCScopedTexture::leak()
{
    setId(0);
}

}
