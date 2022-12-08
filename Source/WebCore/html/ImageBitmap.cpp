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
#include "CSSStyleImageValue.h"
#include "CachedImage.h"
#include "ExceptionCode.h"
#include "ExceptionOr.h"
#include "FileReaderLoader.h"
#include "FileReaderLoaderClient.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "HostWindow.h"
#include "ImageBitmapOptions.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "IntRect.h"
#include "JSDOMPromiseDeferred.h"
#include "JSImageBitmap.h"
#include "LayoutSize.h"
#if ENABLE(OFFSCREEN_CANVAS)
#include "OffscreenCanvas.h"
#endif
#include "RenderElement.h"
#include "SharedBuffer.h"
#include "SuspendableTimer.h"
#include "WebCodecsVideoFrame.h"
#include "WorkerClient.h"
#include "WorkerGlobalScope.h"
#include <variant>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(ImageBitmap);

#if USE(IOSURFACE_CANVAS_BACKING_STORE)
static RenderingMode bufferRenderingMode = RenderingMode::Accelerated;
#else
static RenderingMode bufferRenderingMode = RenderingMode::Unaccelerated;
#endif

Ref<ImageBitmap> ImageBitmap::create(ScriptExecutionContext& scriptExecutionContext, const IntSize& size, DestinationColorSpace colorSpace)
{
    return create({ createImageBuffer(scriptExecutionContext, size, bufferRenderingMode, colorSpace) });
}

Ref<ImageBitmap> ImageBitmap::create(std::optional<ImageBitmapBacking>&& backingStore)
{
    return adoptRef(*new ImageBitmap(WTFMove(backingStore)));
}

RefPtr<ImageBuffer> ImageBitmap::createImageBuffer(ScriptExecutionContext& scriptExecutionContext, const FloatSize& size, RenderingMode renderingMode, DestinationColorSpace colorSpace, float resolutionScale)
{
    // FIXME: Should avoid converting color space and pixel format of image sources.
    auto imageBufferColorSpace = colorSpace.asRGB();
    if (!imageBufferColorSpace) {
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
        imageBufferColorSpace = DestinationColorSpace::DisplayP3();
#else
        imageBufferColorSpace = DestinationColorSpace::SRGB();
#endif
    }

    auto bufferOptions = bufferOptionsForRendingMode(renderingMode);

    GraphicsClient* client = nullptr;
    if (scriptExecutionContext.isDocument()) {
        auto& document = downcast<Document>(scriptExecutionContext);
        if (document.view() && document.view()->root()) {
            client = document.view()->root()->hostWindow();
        }
    } else if (scriptExecutionContext.isWorkerGlobalScope())
        client = downcast<WorkerGlobalScope>(scriptExecutionContext).workerClient();

    return ImageBuffer::create(size, RenderingPurpose::Canvas, resolutionScale, *imageBufferColorSpace, PixelFormat::BGRA8, bufferOptions, { client });
}

void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, ImageBitmap::Source&& source, ImageBitmapOptions&& options, ImageBitmap::Promise&& promise)
{
    WTF::switchOn(source,
        [&] (auto& specificSource) {
            createPromise(scriptExecutionContext, specificSource, WTFMove(options), std::nullopt, WTFMove(promise));
        }
    );
}

RefPtr<ImageBuffer> ImageBitmap::createImageBuffer(ScriptExecutionContext& scriptExecutionContext, const FloatSize& size, DestinationColorSpace colorSpace, float resolutionScale)
{
    return createImageBuffer(scriptExecutionContext, size, bufferRenderingMode, colorSpace, resolutionScale);
}

Vector<std::optional<ImageBitmapBacking>> ImageBitmap::detachBitmaps(Vector<RefPtr<ImageBitmap>>&& bitmaps)
{
    return WTF::map(WTFMove(bitmaps), [](auto&& bitmap) -> std::optional<ImageBitmapBacking> {
        std::optional<ImageBitmapBacking> backing = bitmap->takeImageBitmapBacking();
        if (!backing)
            return std::nullopt;
        if (auto copyBuffer = backing->takeImageBufferForDifferentThread())
            return ImageBitmapBacking(WTFMove(copyBuffer), backing->serializationState());
        return std::nullopt;
    });
}

void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, ImageBitmap::Source&& source, ImageBitmapOptions&& options, int sx, int sy, int sw, int sh, ImageBitmap::Promise&& promise)
{
    // 1. If either the sw or sh arguments are specified but zero, return a promise
    //    rejected with an "RangeError" DOMException and abort these steps.
    if (!sw || !sh) {
        promise.reject(RangeError, "Cannot create ImageBitmap with a width or height of 0"_s);
        return;
    }

    auto left = sw >= 0 ? sx : sx + sw;
    auto top = sh >= 0 ? sy : sy + sh;
    auto width = std::abs(sw);
    auto height = std::abs(sh);

    WTF::switchOn(source,
        [&] (auto& specificSource) {
            createPromise(scriptExecutionContext, specificSource, WTFMove(options), IntRect { left, top, width, height }, WTFMove(promise));
        }
    );
}

static bool taintsOrigin(CachedImage& cachedImage)
{
    auto* image = cachedImage.image();
    if (!image)
        return false;

    if (image->sourceURL().protocolIsData())
        return false;

    if (!image->hasSingleSecurityOrigin())
        return true;

    if (!cachedImage.isCORSSameOrigin())
        return true;

    return false;
}

#if ENABLE(VIDEO)
static bool taintsOrigin(SecurityOrigin* origin, HTMLVideoElement& video)
{
    if (!video.hasSingleSecurityOrigin())
        return true;

    if (!video.player() || video.player()->didPassCORSAccessCheck())
        return false;

    auto url = video.currentSrc();
    if (url.protocolIsData())
        return false;

    return !origin->canRequest(url);
}
#endif

// https://html.spec.whatwg.org/multipage/imagebitmap-and-animations.html#cropped-to-the-source-rectangle-with-formatting
static ExceptionOr<IntRect> croppedSourceRectangleWithFormatting(IntSize inputSize, ImageBitmapOptions& options, std::optional<IntRect> rect)
{
    // 2. If either or both of resizeWidth and resizeHeight members of options are less
    //    than or equal to 0, then return a promise rejected with "InvalidStateError"
    //    DOMException and abort these steps.
    if ((options.resizeWidth && options.resizeWidth.value() <= 0) || (options.resizeHeight && options.resizeHeight.value() <= 0))
        return Exception { InvalidStateError, "Invalid resize dimensions"_s };

    // 3. If sx, sy, sw and sh are specified, let sourceRectangle be a rectangle whose
    //    corners are the four points (sx, sy), (sx+sw, sy),(sx+sw, sy+sh), (sx,sy+sh).
    //    Otherwise let sourceRectangle be a rectangle whose corners are the four points
    //    (0,0), (width of input, 0), (width of input, height of input), (0, height of
    //    input).
    auto sourceRectangle = rect.value_or(IntRect { 0, 0, inputSize.width(), inputSize.height() });

    // 4. Clip sourceRectangle to the dimensions of input.
    sourceRectangle.intersect(IntRect { 0, 0, inputSize.width(), inputSize.height() });

    return { WTFMove(sourceRectangle) };
}

static IntSize outputSizeForSourceRectangle(IntRect sourceRectangle, ImageBitmapOptions& options)
{
    // 5. Let outputWidth be determined as follows:
    auto outputWidth = [&] () -> int {
        if (options.resizeWidth)
            return options.resizeWidth.value();
        if (options.resizeHeight)
            return ceil(sourceRectangle.width() * static_cast<double>(options.resizeHeight.value()) / sourceRectangle.height());
        return sourceRectangle.width();
    }();

    // 6. Let outputHeight be determined as follows:
    auto outputHeight = [&] () -> int {
        if (options.resizeHeight)
            return options.resizeHeight.value();
        if (options.resizeWidth)
            return ceil(sourceRectangle.height() * static_cast<double>(options.resizeWidth.value()) / sourceRectangle.width());
        return sourceRectangle.height();
    }();

    return { outputWidth, outputHeight };
}

static InterpolationQuality interpolationQualityForResizeQuality(ImageBitmapOptions::ResizeQuality resizeQuality)
{
    switch (resizeQuality) {
    case ImageBitmapOptions::ResizeQuality::Pixelated:
        return InterpolationQuality::DoNotInterpolate;
    case ImageBitmapOptions::ResizeQuality::Low:
        return InterpolationQuality::Default; // Low is the default.
    case ImageBitmapOptions::ResizeQuality::Medium:
        return InterpolationQuality::Medium;
    case ImageBitmapOptions::ResizeQuality::High:
        return InterpolationQuality::High;
    }
    ASSERT_NOT_REACHED();
    return InterpolationQuality::Default;
}

static AlphaPremultiplication alphaPremultiplicationForPremultiplyAlpha(ImageBitmapOptions::PremultiplyAlpha premultiplyAlpha)
{
    // The default is to premultiply - this is the least surprising behavior.
    if (premultiplyAlpha == ImageBitmapOptions::PremultiplyAlpha::None)
        return AlphaPremultiplication::Unpremultiplied;
    return AlphaPremultiplication::Premultiplied;
}

void ImageBitmap::resolveWithBlankImageBuffer(ScriptExecutionContext& scriptExecutionContext, bool originClean, Promise&& promise)
{
    // Source rectangle likely doesn't intersect the source image.
    // Behavior isn't well specified, but WPT tests expect no Promise rejection (and of course no crashes).
    // Resolve Promise with a blank 1x1 ImageBitmap.
    auto bitmapData = createImageBuffer(scriptExecutionContext, FloatSize(1, 1), bufferRenderingMode, DestinationColorSpace::SRGB());

    // 9. If the origin of image's image is not the same origin as the origin specified by the
    //    entry settings object, then set the origin-clean flag of the ImageBitmap object's
    //    bitmap to false.
    OptionSet<SerializationState> serializationState;
    if (originClean)
        serializationState.add(SerializationState::OriginClean);
    
    // 7. Create a new ImageBitmap object.
    auto imageBitmap = create(ImageBitmapBacking(WTFMove(bitmapData), serializationState));

    // 10. Return a new promise, but continue running these steps in parallel.
    // 11. Resolve the promise with the new ImageBitmap object as the value.
    promise.resolve(WTFMove(imageBitmap));
}

// FIXME: More steps from https://html.spec.whatwg.org/multipage/imagebitmap-and-animations.html#cropped-to-the-source-rectangle-with-formatting

// 7. Place input on an infinite transparent black grid plane, positioned so that its
//    top left corner is at the origin of the plane, with the x-coordinate increasing
//    to the right, and the y-coordinate increasing down, and with each pixel in the
//    input image data occupying a cell on the plane's grid.

// 8. Let output be the rectangle on the plane denoted by sourceRectangle.

// 9. Scale output to the size specified by outputWidth and outputHeight. The user
//    agent should use the value of the resizeQuality option to guide the choice of
//    scaling algorithm.

// 10. If the value of the imageOrientation member of options is "flipY", output must
//     be flipped vertically, disregarding any image orientation metadata of the source
//     (such as EXIF metadata), if any.

// 11. If image is an img element or a Blob object, let val be the value of the
//     colorSpaceConversion member of options, and then run these substeps:
//
//     1. If val is "default", the color space conversion behavior is implementation-specific,
//        and should be chosen according to the color space that the implementation uses for
//        drawing images onto the canvas.
//
//     2. If val is "none", output must be decoded without performing any color space
//        conversions. This means that the image decoding algorithm must ignore color profile
//        metadata embedded in the source data as well as the display device color profile.

// 12. Let val be the value of premultiplyAlpha member of options, and then run these substeps:
//
//     1. If val is "default", the alpha premultiplication behavior is implementation-specific,
//        and should be chosen according to implementation deems optimal for drawing images
//        onto the canvas.
//
//     2. If val is "premultiply", the output that is not premultiplied by alpha must have its
//        color components multiplied by alpha and that is premultiplied by alpha must be left
//        untouched.
//
//     3. If val is "none", the output that is not premultiplied by alpha must be left untouched
//        and that is premultiplied by alpha must have its color components divided by alpha.

// 13. Return output.

void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, RefPtr<HTMLImageElement>& imageElement, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    // 2. If image is not completely available, then return a promise rejected with
    // an "InvalidStateError" DOMException and abort these steps.

    auto* cachedImage = imageElement->cachedImage();
    if (!cachedImage || !imageElement->complete()) {
        promise.reject(InvalidStateError, "Cannot create ImageBitmap that is not completely available"_s);
        return;
    }

    // 3. If image's media data has no intrinsic dimensions (e.g. it's a vector graphic
    //    with no specified content size), and both or either of the resizeWidth and
    //    resizeHeight options are not specified, then return a promise rejected with
    //    an "InvalidStateError" DOMException and abort these steps.

    auto imageSize = cachedImage->imageSizeForRenderer(imageElement->renderer(), 1.0f);
    if ((!imageSize.width() || !imageSize.height()) && (!options.resizeWidth || !options.resizeHeight)) {
        promise.reject(InvalidStateError, "Cannot create ImageBitmap from a source with no intrinsic size without providing resize dimensions"_s);
        return;
    }

    // 4. If image's media data has no intrinsic dimensions (e.g. it's a vector graphics
    //    with no specified content size), it should be rendered to a bitmap of the size
    //    specified by the resizeWidth and the resizeHeight options.

    if (!imageSize.width() && !imageSize.height()) {
        imageSize.setWidth(options.resizeWidth.value());
        imageSize.setHeight(options.resizeHeight.value());
    }

    // 5. If the sw and sh arguments are not specified and image's media data has both or
    //    either of its intrinsic width and intrinsic height values equal to 0, then return
    //    a promise rejected with an "InvalidStateError" DOMException and abort these steps.
    // 6. If the sh argument is not specified and image's media data has an intrinsic height
    //    of 0, then return a promise rejected with an "InvalidStateError" DOMException and
    //    abort these steps.

    // FIXME: It's unclear how these steps can happen, since step 4 required setting a
    // width and height for the image.

    if (!rect && (!imageSize.width() || !imageSize.height())) {
        promise.reject(InvalidStateError, "Cannot create ImageBitmap from a source with no intrinsic size without providing dimensions"_s);
        return;
    }

    // 8. Let the ImageBitmap object's bitmap data be a copy of image's media data, cropped to
    //    the source rectangle with formatting. If this is an animated image, the ImageBitmap
    //    object's bitmap data must only be taken from the default image of the animation (the
    //    one that the format defines is to be used when animation is not supported or is disabled),
    //    or, if there is no such image, the first frame of the animation.

    auto sourceRectangle = croppedSourceRectangleWithFormatting(roundedIntSize(imageSize), options, WTFMove(rect));
    if (sourceRectangle.hasException()) {
        promise.reject(sourceRectangle.releaseException());
        return;
    }

    auto imageForRenderer = cachedImage->imageForRenderer(imageElement->renderer());
    if (!imageForRenderer) {
        promise.reject(InvalidStateError, "Cannot create ImageBitmap from image that can't be rendered"_s);
        return;
    }

    auto outputSize = outputSizeForSourceRectangle(sourceRectangle.returnValue(), options);
    auto bitmapData = createImageBuffer(scriptExecutionContext, outputSize, bufferRenderingMode, imageForRenderer->colorSpace());
    if (!bitmapData) {
        resolveWithBlankImageBuffer(scriptExecutionContext, !taintsOrigin(*cachedImage), WTFMove(promise));
        return;
    }

    auto orientation = imageForRenderer->orientation();
    if (orientation == ImageOrientation::FromImage)
        orientation = ImageOrientation::None;

    FloatRect destRect(FloatPoint(), outputSize);
    bitmapData->context().drawImage(*imageForRenderer, destRect, sourceRectangle.releaseReturnValue(), { interpolationQualityForResizeQuality(options.resizeQuality), options.resolvedImageOrientation(orientation) });

    // 9. If the origin of image's image is not the same origin as the origin specified by the
    //    entry settings object, then set the origin-clean flag of the ImageBitmap object's
    //    bitmap to false.
    OptionSet<SerializationState> serializationState;
    if (!taintsOrigin(*cachedImage))
        serializationState.add(SerializationState::OriginClean);

    if (alphaPremultiplicationForPremultiplyAlpha(options.premultiplyAlpha) == AlphaPremultiplication::Premultiplied)
        serializationState.add(SerializationState::PremultiplyAlpha);

    // 7. Create a new ImageBitmap object.
    auto imageBitmap = create(ImageBitmapBacking(WTFMove(bitmapData), serializationState));

    // 10. Return a new promise, but continue running these steps in parallel.
    // 11. Resolve the promise with the new ImageBitmap object as the value.

    promise.resolve(WTFMove(imageBitmap));
}

void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, RefPtr<HTMLCanvasElement>& canvasElement, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    createPromise(scriptExecutionContext, *canvasElement, WTFMove(options), WTFMove(rect), WTFMove(promise));
}

#if ENABLE(OFFSCREEN_CANVAS)
void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, RefPtr<OffscreenCanvas>& canvasElement, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    createPromise(scriptExecutionContext, *canvasElement, WTFMove(options), WTFMove(rect), WTFMove(promise));
}
#endif

#if ENABLE(WEB_CODECS)
void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, RefPtr<WebCodecsVideoFrame>& videoFrame, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    if (videoFrame->isDetached()) {
        promise.reject(InvalidStateError, "Cannot create ImageBitmap from a detached video frame"_s);
        return;
    }
    auto internalFrame = videoFrame->internalFrame();
    if (!internalFrame) {
        promise.reject(InvalidStateError, "Cannot create ImageBitmap from an empty video frame"_s);
        return;
    }

    if (!videoFrame->codedWidth() || !videoFrame->codedHeight()) {
        promise.reject(InvalidStateError, "Cannot create ImageBitmap from a video frame that has zero width or height"_s);
        return;
    }

    auto sourceRectangle = croppedSourceRectangleWithFormatting({ static_cast<int>(videoFrame->displayWidth()), static_cast<int>(videoFrame->displayHeight()) }, options, WTFMove(rect));
    if (sourceRectangle.hasException()) {
        promise.reject(sourceRectangle.releaseException());
        return;
    }

    auto outputSize = outputSizeForSourceRectangle(sourceRectangle.returnValue(), options);
    auto bitmapData = createImageBuffer(scriptExecutionContext, outputSize, bufferRenderingMode, DestinationColorSpace::SRGB());

    if (!bitmapData) {
        resolveWithBlankImageBuffer(scriptExecutionContext, true, WTFMove(promise));
        return;
    }

    FloatRect destRect(FloatPoint(), outputSize);
    bitmapData->context().paintVideoFrame(*internalFrame, destRect, true);

    OptionSet<SerializationState> serializationState { SerializationState::OriginClean };
    auto imageBitmap = create(ImageBitmapBacking(WTFMove(bitmapData), serializationState));

    promise.resolve(WTFMove(imageBitmap));
}
#endif

void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, CanvasBase& canvas, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    // 2. If the canvas element's bitmap has either a horizontal dimension or a vertical
    //    dimension equal to zero, then return a promise rejected with an "InvalidStateError"
    //    DOMException and abort these steps.
    auto size = canvas.size();
    if (!size.width() || !size.height()) {
        promise.reject(InvalidStateError, "Cannot create ImageBitmap from a canvas that has zero width or height"_s);
        return;
    }

    // 4. Let the ImageBitmap object's bitmap data be a copy of the canvas element's bitmap
    //    data, cropped to the source rectangle with formatting.

    auto sourceRectangle = croppedSourceRectangleWithFormatting(size, options, WTFMove(rect));
    if (sourceRectangle.hasException()) {
        promise.reject(sourceRectangle.releaseException());
        return;
    }

    auto imageForRender = canvas.copiedImage();
    if (!imageForRender) {
        promise.reject(InvalidStateError, "Cannot create ImageBitmap from canvas that can't be rendered"_s);
        return;
    }

    auto outputSize = outputSizeForSourceRectangle(sourceRectangle.returnValue(), options);
    auto bitmapData = createImageBuffer(scriptExecutionContext, outputSize, bufferRenderingMode, imageForRender->colorSpace());

    if (!bitmapData) {
        resolveWithBlankImageBuffer(scriptExecutionContext, canvas.originClean(), WTFMove(promise));
        return;
    }

    FloatRect destRect(FloatPoint(), outputSize);
    bitmapData->context().drawImage(*imageForRender, destRect, sourceRectangle.releaseReturnValue(), { interpolationQualityForResizeQuality(options.resizeQuality), options.resolvedImageOrientation(ImageOrientation::None) });

    // 5. Set the origin-clean flag of the ImageBitmap object's bitmap to the same value as
    //    the origin-clean flag of the canvas element's bitmap.
    OptionSet<SerializationState> serializationState;
    if (canvas.originClean())
        serializationState.add(SerializationState::OriginClean);

    if (alphaPremultiplicationForPremultiplyAlpha(options.premultiplyAlpha) == AlphaPremultiplication::Premultiplied)
        serializationState.add(SerializationState::PremultiplyAlpha);

    // 3. Create a new ImageBitmap object.
    auto imageBitmap = create(ImageBitmapBacking(WTFMove(bitmapData), serializationState));

    // 6. Return a new promise, but continue running these steps in parallel.
    // 7. Resolve the promise with the new ImageBitmap object as the value.

    promise.resolve(WTFMove(imageBitmap));
}

#if ENABLE(VIDEO)
void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, RefPtr<HTMLVideoElement>& video, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    // https://html.spec.whatwg.org/multipage/#dom-createimagebitmap
    // WHATWG HTML 2102913b313078cd8eeac7e81e6a8756cbd3e773
    // Steps 3-7.
    // (Step 3 is handled in croppedSourceRectangleWithFormatting.)

    // 4. Check the usability of the image argument. If this throws an exception
    //    or returns bad, then return p rejected with an "InvalidStateError"
    //    DOMException.
    if (video->readyState() == HTMLMediaElement::HAVE_NOTHING || video->readyState() == HTMLMediaElement::HAVE_METADATA) {
        promise.reject(InvalidStateError, "Cannot create ImageBitmap before the HTMLVideoElement has data"_s);
        return;
    }

    // 6.1. If image's networkState attribute is NETWORK_EMPTY, then return p
    //      rejected with an "InvalidStateError" DOMException.
    if (video->networkState() == HTMLMediaElement::NETWORK_EMPTY) {
        promise.reject(InvalidStateError, "Cannot create ImageBitmap before the HTMLVideoElement has data"_s);
        return;
    }

    // 6.2. Set imageBitmap's bitmap data to a copy of the frame at the current
    //      playback position, at the media resource's intrinsic width and
    //      intrinsic height (i.e., after any aspect-ratio correction has been
    //      applied), cropped to the source rectangle with formatting.
    auto size = video->player() ? roundedIntSize(video->player()->naturalSize()) : IntSize();
    auto maybeSourceRectangle = croppedSourceRectangleWithFormatting(size, options, WTFMove(rect));
    if (maybeSourceRectangle.hasException()) {
        promise.reject(maybeSourceRectangle.releaseException());
        return;
    }
    auto sourceRectangle = maybeSourceRectangle.releaseReturnValue();

    auto outputSize = outputSizeForSourceRectangle(sourceRectangle, options);

    auto colorSpace = video->colorSpace();
    if (!colorSpace)
        colorSpace = DestinationColorSpace::SRGB();

    // FIXME: Add support for pixel formats to ImageBitmap.
    auto bitmapData = video->createBufferForPainting(outputSize, bufferRenderingMode, *colorSpace, PixelFormat::BGRA8);
    if (!bitmapData) {
        resolveWithBlankImageBuffer(scriptExecutionContext, !taintsOrigin(scriptExecutionContext.securityOrigin(), *video), WTFMove(promise));
        return;
    }

    {
        GraphicsContext& c = bitmapData->context();
        GraphicsContextStateSaver stateSaver(c);
        c.clip(FloatRect(FloatPoint(), outputSize));
        auto scaleX = float(outputSize.width()) / float(sourceRectangle.width());
        auto scaleY = float(outputSize.height()) / float(sourceRectangle.height());
        if (options.orientation == ImageBitmapOptions::Orientation::FlipY) {
            c.scale(FloatSize(scaleX, -scaleY));
            c.translate(IntPoint(-sourceRectangle.location().x(), sourceRectangle.location().y() - outputSize.height()));
        } else {
            c.scale(FloatSize(scaleX, scaleY));
            c.translate(-sourceRectangle.location());
        }
        video->paintCurrentFrameInContext(c, FloatRect(FloatPoint(), size));
    }

    // 6.3. If the origin of image's video is not same origin with entry
    //      settings object's origin, then set the origin-clean flag of
    //      image's bitmap to false.
    OptionSet<SerializationState> serializationState;
    if (!taintsOrigin(scriptExecutionContext.securityOrigin(), *video))
        serializationState.add(SerializationState::OriginClean);

    if (alphaPremultiplicationForPremultiplyAlpha(options.premultiplyAlpha) == AlphaPremultiplication::Premultiplied)
        serializationState.add(SerializationState::PremultiplyAlpha);

    // 5. Let imageBitmap be a new ImageBitmap object.
    auto imageBitmap = create(ImageBitmapBacking(WTFMove(bitmapData), serializationState));

    // 6.4.1. Resolve p with imageBitmap.
    promise.resolve(WTFMove(imageBitmap));
}
#endif

void ImageBitmap::createPromise(ScriptExecutionContext&, RefPtr<CSSStyleImageValue>&, ImageBitmapOptions&&, std::optional<IntRect>, ImageBitmap::Promise&& promise)
{
    promise.reject(InvalidStateError, "Not implemented"_s);
}

void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, RefPtr<ImageBitmap>& existingImageBitmap, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    // 2. If image's [[Detached]] internal slot value is true, return a promise
    //    rejected with an "InvalidStateError" DOMException and abort these steps.
    if (existingImageBitmap->isDetached() || !existingImageBitmap->buffer()) {
        promise.reject(InvalidStateError, "Cannot create ImageBitmap from a detached ImageBitmap"_s);
        return;
    }

    // 4. Let the ImageBitmap object's bitmap data be a copy of the image argument's
    //    bitmap data, cropped to the source rectangle with formatting.
    auto sourceRectangle = croppedSourceRectangleWithFormatting(existingImageBitmap->buffer()->truncatedLogicalSize(), options, WTFMove(rect));
    if (sourceRectangle.hasException()) {
        promise.reject(sourceRectangle.releaseException());
        return;
    }

    auto outputSize = outputSizeForSourceRectangle(sourceRectangle.returnValue(), options);
    auto bitmapData = createImageBuffer(scriptExecutionContext, outputSize, bufferRenderingMode, existingImageBitmap->buffer()->colorSpace());

    if (!bitmapData) {
        resolveWithBlankImageBuffer(scriptExecutionContext, existingImageBitmap->originClean(), WTFMove(promise));
        return;
    }

    auto imageForRender = existingImageBitmap->buffer()->copyImage();

    FloatRect destRect(FloatPoint(), outputSize);
    bitmapData->context().drawImage(*imageForRender, destRect, sourceRectangle.releaseReturnValue(), { interpolationQualityForResizeQuality(options.resizeQuality), options.resolvedImageOrientation(ImageOrientation::None) });

    // 5. Set the origin-clean flag of the ImageBitmap object's bitmap to the same
    //    value as the origin-clean flag of the bitmap of the image argument.
    OptionSet<SerializationState> serializationState;
    if (existingImageBitmap->originClean())
        serializationState.add(SerializationState::OriginClean);

    if (alphaPremultiplicationForPremultiplyAlpha(options.premultiplyAlpha) == AlphaPremultiplication::Premultiplied) {
        serializationState.add(SerializationState::PremultiplyAlpha);

        // At least in the Core Graphics backend, when creating an ImageBitmap from
        // an ImageBitmap, the alpha channel of bitmapData isn't premultiplied even
        // though the alpha mode of the internal surface claims it is. Instruct
        // users of this ImageBitmap to ignore the internal surface's alpha mode.
        serializationState.add(SerializationState::ForciblyPremultiplyAlpha);
    }

    // 3. Create a new ImageBitmap object.
    auto imageBitmap = create(ImageBitmapBacking(WTFMove(bitmapData), serializationState));

    // 6. Return a new promise, but continue running these steps in parallel.
    // 7. Resolve the promise with the new ImageBitmap object as the value.
    promise.resolve(WTFMove(imageBitmap));
}

class ImageBitmapImageObserver final : public RefCounted<ImageBitmapImageObserver>, public ImageObserver {
public:
    static Ref<ImageBitmapImageObserver> create(String mimeType, long long expectedContentLength, const URL& sourceUrl)
    {
        return adoptRef(*new ImageBitmapImageObserver(mimeType, expectedContentLength, sourceUrl));
    }

    URL sourceUrl() const override { return m_sourceUrl; }
    String mimeType() const override { return m_mimeType; }
    long long expectedContentLength() const override { return m_expectedContentLength; }

    void decodedSizeChanged(const Image&, long long) override { }

    void didDraw(const Image&) override { }

    bool canDestroyDecodedData(const Image&) override { return true; }
    void imageFrameAvailable(const Image&, ImageAnimatingState, const IntRect* = nullptr, DecodingStatus = DecodingStatus::Invalid) override { }
    void changedInRect(const Image&, const IntRect* = nullptr) override { }
    void scheduleRenderingUpdate(const Image&) override { }

private:
    ImageBitmapImageObserver(String mimeType, long long expectedContentLength, const URL& sourceUrl)
        : m_mimeType(mimeType)
        , m_expectedContentLength(expectedContentLength)
        , m_sourceUrl(sourceUrl)
    { }

    String m_mimeType;
    long long m_expectedContentLength;
    URL m_sourceUrl;
};

class PendingImageBitmap final : public ActiveDOMObject, public FileReaderLoaderClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static void fetch(ScriptExecutionContext& scriptExecutionContext, RefPtr<Blob>&& blob, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
    {
        if (scriptExecutionContext.activeDOMObjectsAreStopped())
            return;
        auto pendingImageBitmap = new PendingImageBitmap(scriptExecutionContext, WTFMove(blob), WTFMove(options), WTFMove(rect), WTFMove(promise));
        pendingImageBitmap->suspendIfNeeded();
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
        , m_createImageBitmapTimer(&scriptExecutionContext, *this, &PendingImageBitmap::createImageBitmapAndResolvePromise)
    {
        m_createImageBitmapTimer.suspendIfNeeded();
    }

    void start(ScriptExecutionContext& scriptExecutionContext)
    {
        m_blobLoader.start(&scriptExecutionContext, *m_blob);
    }

    // ActiveDOMObject

    const char* activeDOMObjectName() const final
    {
        return "PendingImageBitmap";
    }

    void stop() final
    {
        delete this;
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
        createImageBitmapAndResolvePromiseSoon(m_blobLoader.arrayBufferResult());
    }

    void didFail(ExceptionCode) override
    {
        createImageBitmapAndResolvePromiseSoon(nullptr);
    }

    void createImageBitmapAndResolvePromiseSoon(RefPtr<ArrayBuffer>&& arrayBuffer)
    {
        ASSERT(!m_createImageBitmapTimer.isActive());
        m_arrayBufferToProcess = WTFMove(arrayBuffer);
        m_createImageBitmapTimer.startOneShot(0_s);
    }

    void createImageBitmapAndResolvePromise()
    {
        auto destroyOnExit = makeScopeExit([this] {
            delete this;
        });

        if (!m_arrayBufferToProcess) {
            m_promise.reject(InvalidStateError, "An error occured reading the Blob argument to createImageBitmap"_s);
            return;
        }

        ImageBitmap::createFromBuffer(*scriptExecutionContext(), m_arrayBufferToProcess.releaseNonNull(), m_blob->type(), m_blob->size(), m_blobLoader.url(), WTFMove(m_options), WTFMove(m_rect), WTFMove(m_promise));
    }

    FileReaderLoader m_blobLoader;
    RefPtr<Blob> m_blob;
    ImageBitmapOptions m_options;
    std::optional<IntRect> m_rect;
    ImageBitmap::Promise m_promise;
    SuspendableTimer m_createImageBitmapTimer;
    RefPtr<ArrayBuffer> m_arrayBufferToProcess;
};

void ImageBitmap::createFromBuffer(ScriptExecutionContext& scriptExecutionContext, Ref<ArrayBuffer>&& arrayBuffer, String mimeType, long long expectedContentLength, const URL& sourceURL, ImageBitmapOptions&& options, std::optional<IntRect> rect, Promise&& promise)
{
    if (!arrayBuffer->byteLength()) {
        promise.reject(InvalidStateError, "Cannot create an ImageBitmap from an empty buffer"_s);
        return;
    }

    auto sharedBuffer = SharedBuffer::create(static_cast<const char*>(arrayBuffer->data()), arrayBuffer->byteLength());
    auto observer = ImageBitmapImageObserver::create(mimeType, expectedContentLength, sourceURL);
    auto image = BitmapImage::create(observer.ptr());
    auto result = image->setData(sharedBuffer.copyRef(), true);
    if (result != EncodedDataStatus::Complete || image->isNull()) {
        promise.reject(InvalidStateError, "Cannot decode the data in the argument to createImageBitmap"_s);
        return;
    }

    auto sourceRectangle = croppedSourceRectangleWithFormatting(roundedIntSize(image->size()), options, rect);
    if (sourceRectangle.hasException()) {
        promise.reject(sourceRectangle.releaseException());
        return;
    }

    auto outputSize = outputSizeForSourceRectangle(sourceRectangle.returnValue(), options);
    auto bitmapData = createImageBuffer(scriptExecutionContext, outputSize, bufferRenderingMode, image->colorSpace());
    if (!bitmapData) {
        promise.reject(InvalidStateError, "Cannot create an image buffer from the argument to createImageBitmap"_s);
        return;
    }

    auto orientation = image->orientation();
    if (orientation == ImageOrientation::FromImage)
        orientation = ImageOrientation::None;

    FloatRect destRect(FloatPoint(), outputSize);
    bitmapData->context().drawImage(image, destRect, sourceRectangle.releaseReturnValue(), { interpolationQualityForResizeQuality(options.resizeQuality), options.resolvedImageOrientation(orientation) });

    OptionSet<SerializationState> serializationState = SerializationState::OriginClean;
    if (alphaPremultiplicationForPremultiplyAlpha(options.premultiplyAlpha) == AlphaPremultiplication::Premultiplied)
        serializationState.add(SerializationState::PremultiplyAlpha);

    auto imageBitmap = create(ImageBitmapBacking(WTFMove(bitmapData), serializationState));

    promise.resolve(WTFMove(imageBitmap));
}

void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, RefPtr<Blob>& blob, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    // 2. Return a new promise, but continue running these steps in parallel.
    PendingImageBitmap::fetch(scriptExecutionContext, WTFMove(blob), WTFMove(options), WTFMove(rect), WTFMove(promise));
}

void ImageBitmap::createPromise(ScriptExecutionContext& scriptExecutionContext, RefPtr<ImageData>& imageData, ImageBitmapOptions&& options, std::optional<IntRect> rect, ImageBitmap::Promise&& promise)
{
    // 6.1. Let buffer be image's data attribute value's [[ViewedArrayBuffer]]
    //      internal slot.
    // 6.2. If IsDetachedBuffer(buffer) is true, then return p rejected with an
    //      "InvalidStateError" DOMException.
    if (imageData->data().isDetached()) {
        promise.reject(InvalidStateError, "ImageData's viewed buffer has been detached"_s);
        return;
    }

    // 6.3. Set imageBitmap's bitmap data to image's image data, cropped to the
    //      source rectangle with formatting.
    auto sourceRectangle = croppedSourceRectangleWithFormatting(imageData->size(), options, WTFMove(rect));
    if (sourceRectangle.hasException()) {
        promise.reject(sourceRectangle.releaseException());
        return;
    }

    auto outputSize = outputSizeForSourceRectangle(sourceRectangle.returnValue(), options);
    auto bitmapData = createImageBuffer(scriptExecutionContext, outputSize, bufferRenderingMode, toDestinationColorSpace(imageData->colorSpace()));

    if (!bitmapData) {
        resolveWithBlankImageBuffer(scriptExecutionContext, true, WTFMove(promise));
        return;
    }

    // If no cropping, resizing, flipping, etc. are needed, then simply use the
    // resulting ImageBuffer directly.
    auto alphaPremultiplication = alphaPremultiplicationForPremultiplyAlpha(options.premultiplyAlpha);
    if (sourceRectangle.returnValue().location().isZero() && sourceRectangle.returnValue().size() == imageData->size() && sourceRectangle.returnValue().size() == outputSize && options.orientation == ImageBitmapOptions::Orientation::None) {
        bitmapData->putPixelBuffer(imageData->pixelBuffer(), sourceRectangle.releaseReturnValue(), { }, alphaPremultiplication);
        
        auto imageBitmap = create(ImageBitmapBacking(WTFMove(bitmapData)));
        // The result is implicitly origin-clean, and alpha premultiplication has already been handled.
        promise.resolve(WTFMove(imageBitmap));
        return;
    }

    // 6.3. Set imageBitmap's bitmap data to image's image data, cropped to the
    //      source rectangle with formatting.
    auto tempBitmapData = createImageBuffer(scriptExecutionContext, imageData->size(), bufferRenderingMode, toDestinationColorSpace(imageData->colorSpace()));
    if (!tempBitmapData) {
        resolveWithBlankImageBuffer(scriptExecutionContext, true, WTFMove(promise));
        return;
    }
    tempBitmapData->putPixelBuffer(imageData->pixelBuffer(), IntRect(0, 0, imageData->width(), imageData->height()), { }, alphaPremultiplication);
    FloatRect destRect(FloatPoint(), outputSize);
    bitmapData->context().drawImageBuffer(*tempBitmapData, destRect, sourceRectangle.releaseReturnValue(), { interpolationQualityForResizeQuality(options.resizeQuality), options.resolvedImageOrientation(ImageOrientation::None) });

    // 6.4.1. Resolve p with ImageBitmap.
    auto imageBitmap = create({ WTFMove(bitmapData) });
    // The result is implicitly origin-clean, and alpha premultiplication has already been handled.
    promise.resolve(WTFMove(imageBitmap));
}

ImageBitmap::ImageBitmap(std::optional<ImageBitmapBacking>&& backingStore)
    : m_backingStore(WTFMove(backingStore))
{
    ASSERT_IMPLIES(m_backingStore, m_backingStore->buffer());
    updateMemoryCost();
}

ImageBitmap::~ImageBitmap()
{
}

std::optional<ImageBitmapBacking> ImageBitmap::takeImageBitmapBacking()
{
    auto result = std::exchange(m_backingStore, std::nullopt);
    if (result)
        updateMemoryCost();
    return result;
}

RefPtr<ImageBuffer> ImageBitmap::takeImageBuffer()
{
    if (auto backingStore = takeImageBitmapBacking())
        return backingStore->takeImageBuffer();
    ASSERT(isDetached());
    return nullptr;
}

void ImageBitmap::updateMemoryCost()
{
    if (m_backingStore) {
        if (auto imageBuffer = m_backingStore->buffer()) {
            m_memoryCost = imageBuffer->memoryCost();
            return;
        }
    }
    m_memoryCost = 0;
}

size_t ImageBitmap::memoryCost() const
{
    return m_memoryCost;
}

} // namespace WebCore
