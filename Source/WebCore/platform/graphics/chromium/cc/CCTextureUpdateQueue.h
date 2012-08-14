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

#ifndef CCTextureUpdateQueue_h
#define CCTextureUpdateQueue_h

#include "TextureCopier.h"
#include "TextureUploader.h"
#include <wtf/Deque.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class CCTextureUpdateQueue {
    WTF_MAKE_NONCOPYABLE(CCTextureUpdateQueue);
public:
    CCTextureUpdateQueue();
    virtual ~CCTextureUpdateQueue();

    void appendFullUpload(TextureUploader::Parameters);
    void appendPartialUpload(TextureUploader::Parameters);
    void appendCopy(TextureCopier::Parameters);

    void clearUploads();

    TextureUploader::Parameters takeFirstFullUpload();
    TextureUploader::Parameters takeFirstPartialUpload();
    TextureCopier::Parameters takeFirstCopy();

    size_t fullUploadSize() const { return m_fullEntries.size(); }
    size_t partialUploadSize() const { return m_partialEntries.size(); }
    size_t copySize() const { return m_copyEntries.size(); }

    bool hasMoreUpdates() const;

private:
    Deque<TextureUploader::Parameters> m_fullEntries;
    Deque<TextureUploader::Parameters> m_partialEntries;
    Deque<TextureCopier::Parameters> m_copyEntries;
};

}

#endif // CCTextureUpdateQueue_h
