/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef CCTextureUpdater_h
#define CCTextureUpdater_h

#include "IntRect.h"
#include "LayerTextureUpdater.h"
#include <wtf/Vector.h>

namespace WebCore {

class GraphicsContext3D;
class TextureAllocator;
class TextureCopier;
class TextureUploader;

class CCTextureUpdater {
public:
    CCTextureUpdater();
    ~CCTextureUpdater();

    void appendUpdate(LayerTextureUpdater::Texture*, const IntRect& sourceRect, const IntRect& destRect);
    void appendPartialUpdate(LayerTextureUpdater::Texture*, const IntRect& sourceRect, const IntRect& destRect);
    void appendCopy(unsigned sourceTexture, unsigned destTexture, const IntSize&);
    void appendManagedCopy(unsigned sourceTexture, ManagedTexture* destTexture, const IntSize&);

    bool hasMoreUpdates() const;

    // Update some textures. Returns true if more textures left to process.
    bool update(GraphicsContext3D*, TextureAllocator*, TextureCopier*, TextureUploader*, size_t count);

    void clear();

private:
    struct UpdateEntry {
        LayerTextureUpdater::Texture* texture;
        IntRect sourceRect;
        IntRect destRect;
    };

    struct CopyEntry {
        IntSize size;
        unsigned sourceTexture;
        unsigned destTexture;
    };

    struct ManagedCopyEntry {
        IntSize size;
        unsigned sourceTexture;
        ManagedTexture* destTexture;
    };

    static void appendUpdate(LayerTextureUpdater::Texture*, const IntRect& sourceRect, const IntRect& destRect, Vector<UpdateEntry>&);

    size_t m_entryIndex;
    Vector<UpdateEntry> m_entries;
    Vector<UpdateEntry> m_partialEntries;
    Vector<CopyEntry> m_copyEntries;
    Vector<ManagedCopyEntry> m_managedCopyEntries;
};

}

#endif // CCTextureUpdater_h
