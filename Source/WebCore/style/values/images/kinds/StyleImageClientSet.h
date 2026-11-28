/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/StyleImageClient.h>
#include <wtf/WeakHashCountedSet.h>

namespace WebCore {

class Document;
class Element;
class IntRect;

namespace Style {

class Image;

class ImageClientSet {
public:
    void add(ImageClient& client)
    {
        ASSERT(!m_clients.hasNullReferences());
        m_clients.add(client);
    }
    bool remove(ImageClient& client)
    {
        ASSERT(!m_clients.hasNullReferences());
        return m_clients.remove(client);
    }
    bool contains(const ImageClient& client) const { return m_clients.contains(client); }
    bool isEmpty() const { return m_clients.computeIsEmpty(); }

    void imageChanged(const Image&, const IntRect* changedRect = nullptr) const;
    void imageFinishedLoading(const Image&) const;
    void imageFrameAvailable(const Image&) const;
    void imageContentChanged(const Image&) const;
    void imageAnimationPaused(const Image&) const;
    void scheduleRenderingUpdateForImage(const Image&) const;

    bool frameAvailable(const Image&, bool isAnimating, const IntRect* changedRect) const;

    bool isShowingImageVisibly(const Image&) const;
    bool isShowingImageVisiblyInDocument(const Image&, const Document&) const;
    bool allowsImageAnimation(const Image&) const;

    void collectElementsShowingImage(const Image&, HashSet<Ref<Element>>&) const;

private:
    void forEach(NOESCAPE const auto&) const;
    bool anyOf(NOESCAPE const auto&) const;

    SingleThreadWeakHashCountedSet<ImageClient> m_clients;
};

} // namespace Style
} // namespace WebCore
