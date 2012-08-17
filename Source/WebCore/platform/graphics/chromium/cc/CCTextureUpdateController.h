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

#ifndef CCTextureUpdateController_h
#define CCTextureUpdateController_h

#include "CCTextureUpdateQueue.h"
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

class TextureCopier;
class TextureUploader;

class CCTextureUpdateController {
    WTF_MAKE_NONCOPYABLE(CCTextureUpdateController);
public:
    static PassOwnPtr<CCTextureUpdateController> create(PassOwnPtr<CCTextureUpdateQueue> queue, CCResourceProvider* resourceProvider, TextureCopier* copier, TextureUploader* uploader)
    {
        return adoptPtr(new CCTextureUpdateController(queue, resourceProvider, copier, uploader));
    }
    static size_t maxPartialTextureUpdates();
    static void updateTextures(CCResourceProvider*, TextureCopier*, TextureUploader*, CCTextureUpdateQueue*, size_t count);

    virtual ~CCTextureUpdateController();

    bool hasMoreUpdates() const;
    void updateMoreTextures();

    // Virtual for testing.
    virtual size_t updateMoreTexturesSize() const;

protected:
    CCTextureUpdateController(PassOwnPtr<CCTextureUpdateQueue>, CCResourceProvider*, TextureCopier*, TextureUploader*);

    OwnPtr<CCTextureUpdateQueue> m_queue;
    bool m_contentsTexturesPurged;
    CCResourceProvider* m_resourceProvider;
    TextureCopier* m_copier;
    TextureUploader* m_uploader;
};

}

#endif // CCTextureUpdateController_h
