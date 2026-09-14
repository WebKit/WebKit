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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include <wtf/AbstractRefCounted.h>
#include <wtf/Noncopyable.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class IntRect;

enum class ImageAnimatingState : bool;
enum class VisibleInViewportState : uint8_t;

namespace Style {

class Image;
class CachedImage;

class ImageClient : public CanMakeSingleThreadWeakPtr<ImageClient>, public AbstractRefCounted {
    WTF_MAKE_NONCOPYABLE(ImageClient);
public:
    virtual ~ImageClient();

    virtual void imageChanged(const Image&, const IntRect*) const;

    // Called for any Style::CachedImage children.
    virtual void notifyFinished(const CachedImage&) const;
    virtual bool allowsAnimation(const CachedImage&) const;
    virtual bool canDestroyDecodedData(const CachedImage&) const;
    virtual bool useSystemDarkAppearance(const CachedImage&) const;
    virtual VisibleInViewportState imageFrameAvailable(const CachedImage&, ImageAnimatingState, const IntRect*) const;
    virtual VisibleInViewportState imageVisibleInViewport(const CachedImage&, const Document&) const;
    virtual void didRemoveCachedImageClient(const CachedImage&) const;
    virtual void imageContentChanged(const CachedImage&) const;
    virtual void scheduleRenderingUpdateForImage(const CachedImage&) const;

    virtual bool isRendererClient() const;

protected:
    ImageClient();
};

} // namespace Style
} // namespace WebCore
