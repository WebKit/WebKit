/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#ifndef ManagedTexture_h
#define ManagedTexture_h

#include "IntSize.h"
#include "TextureManager.h"

#include <wtf/FastAllocBase.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class GraphicsContext3D;

class ManagedTexture {
    WTF_MAKE_NONCOPYABLE(ManagedTexture);
public:
    static PassOwnPtr<ManagedTexture> create(TextureManager* manager)
    {
        return adoptPtr(new ManagedTexture(manager));
    }
    ~ManagedTexture();

    bool isValid(const IntSize&, unsigned format);
    bool reserve(const IntSize&, unsigned format);
    void unreserve();
    bool isReserved()
    {
        ASSERT(m_textureManager);
        return m_textureManager->isProtected(m_token);
    }

    void allocate(TextureAllocator*);
    void bindTexture(GraphicsContext3D*, TextureAllocator*);
    void framebufferTexture2D(GraphicsContext3D*, TextureAllocator*);

    IntSize size() const { return m_size; }
    unsigned format() const { return m_format; }
    unsigned textureId() const { return m_textureId; }

private:
    explicit ManagedTexture(TextureManager*);

    TextureManager* m_textureManager;
    TextureToken m_token;
    IntSize m_size;
    unsigned m_format;
    unsigned m_textureId;
};

}

#endif // ManagedTexture_h
