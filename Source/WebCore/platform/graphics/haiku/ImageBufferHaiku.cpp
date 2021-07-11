/*
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageBufferHaikuSurfaceBackend.h"

#include "BitmapImage.h"
#include "ColorUtilities.h"
#include "GraphicsContextHaiku.h"
#include "ImageData.h"
#include "IntRect.h"
#include "MIMETypeRegistry.h"
#include "NotImplemented.h"
#include "JavaScriptCore/JSCInlines.h"
#include "JavaScriptCore/TypedArrayInlines.h"

#include <wtf/text/Base64.h>
#include <wtf/text/CString.h>
#include <BitmapStream.h>
#include <Picture.h>
#include <String.h>
#include <TranslatorRoster.h>
#include <stdio.h>


namespace WebCore {


ImageBufferData::ImageBufferData(const IntSize& size)
    : m_view(NULL)
    , m_context(NULL)
    , m_image(adoptRef(new BitmapRef(BRect(0, 0, size.width() - 1, size.height() - 1), B_RGBA32, true)))
{
    // Always keep the bitmap locked, we are the only client.
    m_image->Lock();
    if(size.isEmpty())
        return;

    if (!m_image->IsLocked() || !m_image->IsValid())
        return;

    m_view = new BView(m_image->Bounds(), "WebKit ImageBufferData", 0, 0);
    m_image->AddChild(m_view);

    // Fill with completely transparent color.
    memset(m_image->Bits(), 0, m_image->BitsLength());

    // Since ImageBuffer is used mainly for Canvas, explicitly initialize
    // its view's graphics state with the corresponding canvas defaults
    // NOTE: keep in sync with CanvasRenderingContext2D::State
    m_view->SetLineMode(B_BUTT_CAP, B_MITER_JOIN, 10);
    m_view->SetDrawingMode(B_OP_ALPHA);
    m_view->SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_COMPOSITE);
    m_context = new GraphicsContextHaiku(m_view);
}


ImageBufferData::~ImageBufferData()
{
    m_view = nullptr;
        // m_bitmap owns m_view and deletes it when going out of this destructor.
    m_image = nullptr;
    delete m_context;
}


WTF::RefPtr<WebCore::NativeImage> ImageBufferHaikuSurfaceBackend::copyNativeImage(WebCore::BackingStoreCopy doCopy) const
{
    if (doCopy == DontCopyBackingStore) {
        PlatformImagePtr ref = m_data.m_image;
        return NativeImage::create(std::move(ref));
    } else {
        BitmapRef* ref = new BitmapRef(*(BBitmap*)m_data.m_image.get());
        return NativeImage::create(ref);
    }
}


std::unique_ptr<ImageBufferHaikuSurfaceBackend>
ImageBufferHaikuSurfaceBackend::create(const ImageBufferBackend::Parameters& parameters, const WebCore::HostWindow*)
{
    IntSize backendSize = calculateBackendSize(parameters);
    if (backendSize.isEmpty())
        return nullptr;

    return std::unique_ptr<ImageBufferHaikuSurfaceBackend>(
        new ImageBufferHaikuSurfaceBackend(parameters, backendSize));
}


std::unique_ptr<ImageBufferHaikuSurfaceBackend>
ImageBufferHaikuSurfaceBackend::create(const ImageBufferBackend::Parameters& parameters,
    const GraphicsContext&)
{
    return create(parameters, NULL);
}


ImageBufferHaikuSurfaceBackend::ImageBufferHaikuSurfaceBackend(
    const Parameters& parameters, const IntSize& backendSize)
    : ImageBufferBackend(parameters)
    , m_data(backendSize)
{
}

ImageBufferHaikuSurfaceBackend::~ImageBufferHaikuSurfaceBackend()
{
}

GraphicsContext& ImageBufferHaikuSurfaceBackend::context() const
{
    return *m_data.m_context;
}

RefPtr<Image> ImageBufferHaikuSurfaceBackend::copyImage(BackingStoreCopy copyBehavior, PreserveResolution) const
{
    if (m_data.m_view)
        m_data.m_view->Sync();

    return BitmapImage::create(copyNativeImage(copyBehavior));
}

void ImageBufferHaikuSurfaceBackend::draw(GraphicsContext& destContext,
    const FloatRect& destRect, const FloatRect& srcRect,
    const ImagePaintingOptions& options)
{
    if (!m_data.m_view)
        return;

    m_data.m_view->Sync();
    if (&destContext == &context() && destRect.intersects(srcRect)) {
        // We're drawing into our own buffer.  In order for this to work, we need to copy the source buffer first.
        RefPtr<Image> copy = copyImage(CopyBackingStore, PreserveResolution::Yes);
        destContext.drawImage(*copy.get(), destRect, srcRect, options);
    } else {
        dynamic_cast<GraphicsContextHaiku&>(destContext).drawBitmap(m_data.m_image.get(), srcRect.size(), destRect, srcRect, options);
    }
}

void ImageBufferHaikuSurfaceBackend::drawPattern(GraphicsContext& destContext,
    const FloatRect& destRect, const FloatRect& srcRect,
    const AffineTransform& patternTransform, const FloatPoint& phase,
    const FloatSize& size, const ImagePaintingOptions& options)
{
    if (!m_data.m_view)
        return;

    m_data.m_view->Sync();
    if (&destContext == &context() && srcRect.intersects(destRect)) {
        // We're drawing into our own buffer.  In order for this to work, we need to copy the source buffer first.
        RefPtr<Image> copy = copyImage(CopyBackingStore, PreserveResolution::Yes);
        copy->drawPattern(destContext, destRect, srcRect, patternTransform, phase, size, options.compositeOperator());
    } else
        dynamic_cast<GraphicsContextHaiku&>(destContext).drawBitmap(m_data.m_image.get(), srcRect.size(), destRect, srcRect, patternTransform, phase, size, options.compositeOperator());
}


std::optional<PixelBuffer> ImageBufferHaikuSurfaceBackend::getPixelBuffer(const PixelBufferFormat& outputFormat, const IntRect& srcRect) const
{
    return ImageBufferBackend::getPixelBuffer(outputFormat, srcRect, m_data.m_image->Bits());
}

void ImageBufferHaikuSurfaceBackend::putPixelBuffer(const PixelBuffer& imageData, const IntRect& sourceRect, const IntPoint& destPoint, AlphaPremultiplication premultiplication)
{
    ImageBufferBackend::putPixelBuffer(imageData, sourceRect, destPoint, premultiplication, m_data.m_image->Bits());
}

unsigned ImageBufferHaikuSurfaceBackend::bytesPerRow() const
{
    return m_data.m_image->BytesPerRow();
}

// TODO: PreserveResolution
String ImageBufferHaikuSurfaceBackend::toDataURL(const String& mimeType, std::optional<double> quality, PreserveResolution) const
{
    if (!MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType))
        return "data:,";

    Vector<uint8_t> binaryBuffer = toData(mimeType, quality);

    if (binaryBuffer.size() == 0)
        return "data:,";

    Vector<char> encodedBuffer;

    return makeString("data:", mimeType, ";base64;",
        base64Encoded(binaryBuffer.data(), binaryBuffer.size()));
}


// TODO: quality
Vector<uint8_t> ImageBufferHaikuSurfaceBackend::toData(const String& mimeType, std::optional<double> /*quality*/) const
{
    BString mimeTypeString(mimeType);

    uint32 translatorType = 0;

    BTranslatorRoster* roster = BTranslatorRoster::Default();
    translator_id* translators;
    int32 translatorCount;
    roster->GetAllTranslators(&translators, &translatorCount);
    for (int32 i = 0; i < translatorCount; i++) {
        // Skip translators that don't support archived BBitmaps as input data.
        const translation_format* inputFormats;
        int32 formatCount;
        roster->GetInputFormats(translators[i], &inputFormats, &formatCount);
        bool supportsBitmaps = false;
        for (int32 j = 0; j < formatCount; j++) {
            if (inputFormats[j].type == B_TRANSLATOR_BITMAP) {
                supportsBitmaps = true;
                break;
            }
        }
        if (!supportsBitmaps)
            continue;

        const translation_format* outputFormats;
        roster->GetOutputFormats(translators[i], &outputFormats, &formatCount);
        for (int32 j = 0; j < formatCount; j++) {
            if (outputFormats[j].group == B_TRANSLATOR_BITMAP
                && mimeTypeString == outputFormats[j].MIME) {
                translatorType = outputFormats[j].type;
            }
        }
        if (translatorType)
            break;
    }


    BMallocIO translatedStream;
        // BBitmapStream doesn't take "const Bitmap*"...
    BBitmapStream bitmapStream(m_data.m_image.get());
    BBitmap* tmp = NULL;
    if (roster->Translate(&bitmapStream, 0, 0, &translatedStream, translatorType,
                          B_TRANSLATOR_BITMAP, mimeType.utf8().data()) != B_OK) {
        bitmapStream.DetachBitmap(&tmp);
        return Vector<uint8_t>();
    }

    bitmapStream.DetachBitmap(&tmp);

    // FIXME we could use a BVectorIO to avoid an extra copy here
    Vector<uint8_t> result;
    off_t size;
    translatedStream.GetSize(&size);
    result.append((uint8_t*)translatedStream.Buffer(), size);

    return result;
}

size_t ImageBufferHaikuSurfaceBackend::calculateMemoryCost(const Parameters& parameters)
{
    IntSize backendSize = calculateBackendSize(parameters);
    int bytesPerRow = backendSize.width() * 4;
    return ImageBufferBackend::calculateMemoryCost(backendSize, bytesPerRow);
}

} // namespace WebCore

