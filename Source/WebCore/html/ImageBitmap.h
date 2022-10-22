/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "IDLTypes.h"
#include "ImageBitmapBacking.h"
#include "ScriptWrappable.h"
#include <atomic>
#include <wtf/RefCounted.h>

namespace JSC {
class ArrayBuffer;
}

using JSC::ArrayBuffer;

namespace WebCore {

class Blob;
class CanvasBase;
class CSSStyleImageValue;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBitmapImageObserver;
class ImageData;
class IntRect;
class IntSize;
#if ENABLE(OFFSCREEN_CANVAS)
class OffscreenCanvas;
#endif
class PendingImageBitmap;
class ScriptExecutionContext;
#if ENABLE(WEB_CODECS)
class WebCodecsVideoFrame;
#endif

struct ImageBitmapOptions;

template<typename IDLType> class DOMPromiseDeferred;

class ImageBitmap final : public ScriptWrappable, public RefCounted<ImageBitmap> {
    WTF_MAKE_ISO_ALLOCATED(ImageBitmap);
public:
    using Source = std::variant<
        RefPtr<HTMLImageElement>,
#if ENABLE(VIDEO)
        RefPtr<HTMLVideoElement>,
#endif
        RefPtr<HTMLCanvasElement>,
        RefPtr<ImageBitmap>,
#if ENABLE(OFFSCREEN_CANVAS)
        RefPtr<OffscreenCanvas>,
#endif
        RefPtr<CSSStyleImageValue>,
#if ENABLE(WEB_CODECS)
        RefPtr<WebCodecsVideoFrame>,
#endif
        RefPtr<Blob>,
        RefPtr<ImageData>
    >;

    using Promise = DOMPromiseDeferred<IDLInterface<ImageBitmap>>;

    static void createPromise(ScriptExecutionContext&, Source&&, ImageBitmapOptions&&, Promise&&);
    static void createPromise(ScriptExecutionContext&, Source&&, ImageBitmapOptions&&, int sx, int sy, int sw, int sh, Promise&&);

    static RefPtr<ImageBuffer> createImageBuffer(ScriptExecutionContext&, const FloatSize&, RenderingMode, DestinationColorSpace, float resolutionScale = 1);

    static Ref<ImageBitmap> create(ScriptExecutionContext&, const IntSize&, DestinationColorSpace);
    static Ref<ImageBitmap> create(std::optional<ImageBitmapBacking>&&);

    ~ImageBitmap();

    ImageBuffer* buffer() const { return m_backingStore ? m_backingStore->buffer() : nullptr; }
    // This function has the implicit side-effect of detaching the backing store.
    // It returns nullptr if the ImageBitmap's already detached.
    RefPtr<ImageBuffer> takeImageBuffer();
    OptionSet<SerializationState> serializationState() const { return m_backingStore ? m_backingStore->serializationState() : SerializationState(); }

    unsigned width() const { return m_backingStore ? m_backingStore->width() : 0; }
    unsigned height() const { return m_backingStore ? m_backingStore->height() : 0; }

    bool originClean() const { return m_backingStore && m_backingStore->originClean(); }
    bool premultiplyAlpha() const { return m_backingStore && m_backingStore->premultiplyAlpha(); }
    bool forciblyPremultiplyAlpha() const { return m_backingStore && m_backingStore->forciblyPremultiplyAlpha(); }

    std::optional<ImageBitmapBacking> takeImageBitmapBacking();
    bool isDetached() const { return !m_backingStore; }
    void close() { takeImageBitmapBacking(); }

    static Vector<std::optional<ImageBitmapBacking>> detachBitmaps(Vector<RefPtr<ImageBitmap>>&&);

    size_t memoryCost() const;
private:
    friend class ImageBitmapImageObserver;
    friend class PendingImageBitmap;

    ImageBitmap(std::optional<ImageBitmapBacking>&&);

    static void resolveWithBlankImageBuffer(ScriptExecutionContext&, bool originClean, Promise&&);

    static void createPromise(ScriptExecutionContext&, RefPtr<HTMLImageElement>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
#if ENABLE(VIDEO)
    static void createPromise(ScriptExecutionContext&, RefPtr<HTMLVideoElement>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
#endif
    static void createPromise(ScriptExecutionContext&, RefPtr<ImageBitmap>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
    static void createPromise(ScriptExecutionContext&, RefPtr<HTMLCanvasElement>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
#if ENABLE(OFFSCREEN_CANVAS)
    static void createPromise(ScriptExecutionContext&, RefPtr<OffscreenCanvas>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
#endif
#if ENABLE(WEB_CODECS)
    static void createPromise(ScriptExecutionContext&, RefPtr<WebCodecsVideoFrame>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
#endif
    static void createPromise(ScriptExecutionContext&, CanvasBase&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
    static void createPromise(ScriptExecutionContext&, RefPtr<Blob>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
    static void createPromise(ScriptExecutionContext&, RefPtr<ImageData>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
    static void createPromise(ScriptExecutionContext&, RefPtr<CSSStyleImageValue>&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
    static void createFromBuffer(ScriptExecutionContext&, Ref<ArrayBuffer>&&, String mimeType, long long expectedContentLength, const URL&, ImageBitmapOptions&&, std::optional<IntRect>, Promise&&);
    void updateMemoryCost();

    std::optional<ImageBitmapBacking> m_backingStore;
    std::atomic<size_t> m_memoryCost { 0 };
};

}
