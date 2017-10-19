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

#include "config.h"
#include "ImageBitmap.h"

#include "BitmapImage.h"
#include "Blob.h"
#include "CachedImage.h"
#include "ExceptionOr.h"
#include "FileReaderLoader.h"
#include "FileReaderLoaderClient.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBitmapOptions.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "IntRect.h"
#include "JSImageBitmap.h"
#include "LayoutSize.h"
#include "SharedBuffer.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

Ref<ImageBitmap> ImageBitmap::create()
{
    return adoptRef(*new ImageBitmap);
}

void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, ImageBitmap::Source&& source, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    WTF::switchOn(source,
        [&] (auto& specificSource) {
            createPromise(scriptExecutionContext, specificSource, WTFMove(options), std::nullopt, WTFMove(promise));
        }
    );
}

void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, ImageBitmap::Source&& source, ImageBitmapOptions&& options, int sx, int sy, int sw, int sh, ImageBitmap::Promise&& promise)
{
    // 1. If either the sw or sh arguments are specified but zero, return a promise
    //    rejected with an "RangeError" DOMException and abort these steps.
    if (!sw || !sh) {
        promise.reject(RangeError, "Cannot create ImageBitmap with a width or height of 0");
        return;
    }

    WTF::switchOn(source,
        [&] (auto& specificSource) {
            createPromise(scriptExecutionContext, specificSource, WTFMove(options), IntRect { sx, sy, sw, sh }, WTFMove(promise));
        }
    );
}

void ImageBitmap::createPromise(ScriptExecutionContext&, RefPtr<HTMLImageElement>& imageElement, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    UNUSED_PARAM(imageElement);
    UNUSED_PARAM(options);
    UNUSED_PARAM(rect);

    // 2. If image is not completely available, then return a promise rejected with
    // an "InvalidStateError" DOMException and abort these steps.

    // 3. If image's media data has no intrinsic dimensions (e.g. it's a vector graphic
    //    with no specified content size), and both or either of the resizeWidth and
    //    resizeHeight options are not specified, then return a promise rejected with
    //    an "InvalidStateError" DOMException and abort these steps.

    // 4. If image's media data has no intrinsic dimensions (e.g. it's a vector graphics
    //    with no specified content size), it should be rendered to a bitmap of the size
    //    specified by the resizeWidth and the resizeHeight options.

    // 5. If the sw and sh arguments are not specified and image's media data has both or
    //    either of its intrinsic width and intrinsic height values equal to 0, then return
    //    a promise rejected with an "InvalidStateError" DOMException and abort these steps.

    // 6. If the sh argument is not specified and image's media data has an intrinsic height
    //    of 0, then return a promise rejected with an "InvalidStateError" DOMException and
    //    abort these steps.

    // 7. Create a new ImageBitmap object.
    auto imageBitmap = create();

    // 8. Let the ImageBitmap object's bitmap data be a copy of image's media data, cropped to
    //    the source rectangle with formatting. If this is an animated image, the ImageBitmap
    //    object's bitmap data must only be taken from the default image of the animation (the
    //    one that the format defines is to be used when animation is not supported or is disabled),
    //    or, if there is no such image, the first frame of the animation.

    // 9. If the origin of image's image is not the same origin as the origin specified by the
    //    entry settings object, then set the origin-clean flag of the ImageBitmap object's
    //    bitmap to false.

    // 10. Return a new promise, but continue running these steps in parallel.

    // 11. Resolve the promise with the new ImageBitmap object as the value.
    return promise.resolve(WTFMove(imageBitmap));
}

void ImageBitmap::createPromise(ScriptExecutionContext&, RefPtr<HTMLCanvasElement>& canvasElement, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    UNUSED_PARAM(canvasElement);
    UNUSED_PARAM(options);
    UNUSED_PARAM(rect);

    // 2. If the canvas element's bitmap has either a horizontal dimension or a vertical
    //    dimension equal to zero, then return a promise rejected with an "InvalidStateError"
    //    DOMException and abort these steps.

    // 3. Create a new ImageBitmap object.
    auto imageBitmap = create();

    // 4. Let the ImageBitmap object's bitmap data be a copy of the canvas element's bitmap
    //    data, cropped to the source rectangle with formatting.

    // 5. Set the origin-clean flag of the ImageBitmap object's bitmap to the same value as
    //    the origin-clean flag of the canvas element's bitmap.

    // 6. Return a new promise, but continue running these steps in parallel.

    // 7. Resolve the promise with the new ImageBitmap object as the value.
    return promise.resolve(WTFMove(imageBitmap));
}

void ImageBitmap::createPromise(ScriptExecutionContext&, RefPtr<HTMLVideoElement>& videoElement, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    UNUSED_PARAM(videoElement);
    UNUSED_PARAM(options);
    UNUSED_PARAM(rect);

    // 2. If the video element's networkState attribute is NETWORK_EMPTY, then return
    //    a promise rejected with an "InvalidStateError" DOMException and abort these
    //    steps.

    // 3. If the video element's readyState attribute is either HAVE_NOTHING or
    //    HAVE_METADATA, then return a promise rejected with an "InvalidStateError"
    //    DOMException and abort these steps.

    // 4. Create a new ImageBitmap object.
    auto imageBitmap = create();

    // 5. Let the ImageBitmap object's bitmap data be a copy of the frame at the current
    //    playback position, at the media resource's intrinsic width and intrinsic height
    //    (i.e. after any aspect-ratio correction has been applied), cropped to the source
    //    rectangle with formatting.

    // 6. If the origin of the video element is not the same origin as the origin specified
    //    by the entry settings object, then set the origin-clean flag of the ImageBitmap
    //    object's bitmap to false.

    // 7. Return a new promise, but continue running these steps in parallel.

    // 8. Resolve the promise with the new ImageBitmap object as the value.
    return promise.resolve(WTFMove(imageBitmap));
}

void ImageBitmap::createPromise(ScriptExecutionContext&, RefPtr<ImageBitmap>& existingImageBitmap, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    UNUSED_PARAM(existingImageBitmap);
    UNUSED_PARAM(options);
    UNUSED_PARAM(rect);

    // 2. If image's [[Detached]] internal slot value is true, return a promise
    //    rejected with an "InvalidStateError" DOMException and abort these steps.

    // 3. Create a new ImageBitmap object.
    auto imageBitmap = create();

    // 4. Let the ImageBitmap object's bitmap data be a copy of the image argument's
    //    bitmap data, cropped to the source rectangle with formatting.

    // 5. Set the origin-clean flag of the ImageBitmap object's bitmap to the same
    //    value as the origin-clean flag of the bitmap of the image argument.

    // 6. Return a new promise, but continue running these steps in parallel.

    // 7. Resolve the promise with the new ImageBitmap object as the value.
    return promise.resolve(WTFMove(imageBitmap));
}

class PendingImageBitmap final : public ActiveDOMObject, public FileReaderLoaderClient {
public:
    static void fetch(ScriptExecutionContext& scriptExecutionContext, RefPtr<Blob>&& blob, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
    {
        auto pendingImageBitmap = new PendingImageBitmap(scriptExecutionContext, WTFMove(blob), WTFMove(options), WTFMove(rect), WTFMove(promise));
        pendingImageBitmap->start(scriptExecutionContext);
    }

private:
    PendingImageBitmap(ScriptExecutionContext& scriptExecutionContext, RefPtr<Blob>&& blob, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
        : ActiveDOMObject(&scriptExecutionContext)
        , m_blobLoader(FileReaderLoader::ReadAsArrayBuffer, this)
        , m_blob(WTFMove(blob))
        , m_options(WTFMove(options))
        , m_rect(WTFMove(rect))
        , m_promise(WTFMove(promise))
    {
    }

    void start(ScriptExecutionContext& scriptExecutionContext)
    {
        m_blobLoader.start(&scriptExecutionContext, *m_blob);
    }

    // ActiveDOMObject

    const char* activeDOMObjectName() const override
    {
        return "PendingImageBitmap";
    }

    bool canSuspendForDocumentSuspension() const override
    {
        // FIXME: Deal with suspension.
        return false;
    }

    // FileReaderLoaderClient

    void didStartLoading() override
    {
    }

    void didReceiveData() override
    {
    }

    void didFinishLoading() override
    {
        createImageBitmap(m_blobLoader.arrayBufferResult());
        delete this;
    }

    void didFail(int) override
    {
        createImageBitmap(nullptr);
        delete this;
    }

    void createImageBitmap(RefPtr<ArrayBuffer> arrayBuffer)
    {
        UNUSED_PARAM(arrayBuffer);

        // 3. Read the Blob object's data. If an error occurs during reading of the object,
        //    then reject the promise with an "InvalidStateError" DOMException, and abort
        //    these steps.

        // 4. Apply the image sniffing rules to determine the file format of the image data,
        //    with MIME type of the Blob (as given by the Blob object's type attribute) giving
        //    the official type.

        // 5. If the image data is not in a supported image file format (e.g. it's not an image
        //    at all), or if the image data is corrupted in some fatal way such that the image
        //    dimensions cannot be obtained (e.g. a vector graphic with no intrinsic size), then
        //    reject the promise with an "InvalidStateError" DOMException, and abort these steps.

        // 6. Create a new ImageBitmap object.
        auto imageBitmap = ImageBitmap::create();

        // 7. Let the ImageBitmap object's bitmap data be the image data read from the Blob object,
        //    cropped to the source rectangle with formatting. If this is an animated image, the
        //    ImageBitmap object's bitmap data must only be taken from the default image of the
        //    animation (the one that the format defines is to be used when animation is not supported
        //    or is disabled), or, if there is no such image, the first frame of the animation.

        // 8. Resolve the promise with the new ImageBitmap object as the value.
        m_promise.resolve(WTFMove(imageBitmap));
    }

    FileReaderLoader m_blobLoader;
    RefPtr<Blob> m_blob;
    ImageBitmapOptions m_options;
    std::optional<IntRect> m_rect;
    ImageBitmap::Promise m_promise;
};

void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, RefPtr<Blob>& blob, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    // 2. Return a new promise, but continue running these steps in parallel.
    PendingImageBitmap::fetch(scriptExecutionContext, WTFMove(blob), WTFMove(options), WTFMove(rect), WTFMove(promise));
}

void ImageBitmap::createPromise(ScriptExecutionContext&, RefPtr<ImageData>& imageData, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    UNUSED_PARAM(imageData);
    UNUSED_PARAM(options);
    UNUSED_PARAM(rect);

    // 2. If the image object's data attribute value's [[Detached]] internal slot value
    //    is true, return a promise rejected with an "InvalidStateError" DOMException
    //    and abort these steps.

    // 3. Create a new ImageBitmap object.
    auto imageBitmap = create();

    // 4. Let the ImageBitmap object's bitmap data be the image data given by the ImageData
    //    object, cropped to the source rectangle with formatting.

    // 5. Return a new promise, but continue running these steps in parallel.
    // 6. Resolve the promise with the new ImageBitmap object as the value.
    promise.resolve(imageBitmap);
}

ImageBitmap::ImageBitmap() = default;

ImageBitmap::~ImageBitmap() = default;

unsigned ImageBitmap::width() const
{
    if (m_detached || !m_bitmapData)
        return 0;

    // FIXME: Is this the right width?
    return m_bitmapData->logicalSize().width();
}

unsigned ImageBitmap::height() const
{
    if (m_detached || !m_bitmapData)
        return 0;

    // FIXME: Is this the right height?
    return m_bitmapData->logicalSize().height();
}

void ImageBitmap::close()
{
    m_detached = true;
    m_bitmapData = nullptr;
}

}
