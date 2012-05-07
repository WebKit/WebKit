/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#ifndef ThrottledTextureUploader_h
#define ThrottledTextureUploader_h

#include "TextureUploader.h"

#include <wtf/Deque.h>

namespace WebCore {

class ThrottledTextureUploader : public TextureUploader {
    WTF_MAKE_NONCOPYABLE(ThrottledTextureUploader);
public:
    static PassOwnPtr<ThrottledTextureUploader> create(PassRefPtr<GraphicsContext3D> context)
    {
        return adoptPtr(new ThrottledTextureUploader(context));
    }
    static PassOwnPtr<ThrottledTextureUploader> create(PassRefPtr<GraphicsContext3D> context, size_t pendingUploadLimit)
    {
        return adoptPtr(new ThrottledTextureUploader(context, pendingUploadLimit));
    }
    virtual ~ThrottledTextureUploader();

    virtual bool isBusy() OVERRIDE;
    virtual void beginUploads() OVERRIDE;
    virtual void endUploads() OVERRIDE;
    virtual void uploadTexture(GraphicsContext3D*, LayerTextureUpdater::Texture*, TextureAllocator*, const IntRect sourceRect, const IntRect destRect) OVERRIDE;

private:
    class Query {
    public:
        static PassOwnPtr<Query> create(PassRefPtr<GraphicsContext3D> context) { return adoptPtr(new Query(context)); }

        virtual ~Query();

        void begin();
        void end();
        bool isPending();
        void wait();

    private:
        explicit Query(PassRefPtr<GraphicsContext3D>);

        RefPtr<GraphicsContext3D> m_context;
        unsigned m_queryId;
    };

    ThrottledTextureUploader(PassRefPtr<GraphicsContext3D>);
    ThrottledTextureUploader(PassRefPtr<GraphicsContext3D>, size_t pendingUploadLimit);

    void processQueries();

    RefPtr<GraphicsContext3D> m_context;
    size_t m_maxPendingQueries;
    Deque<OwnPtr<Query> > m_pendingQueries;
    Deque<OwnPtr<Query> > m_availableQueries;
};

}

#endif
