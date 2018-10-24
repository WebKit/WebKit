/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ImageDecoderDirect2D.h"

#if USE(DIRECT2D)

#include "GraphicsContext.h"
#include "ImageOrientation.h"
#include "IntPoint.h"
#include "IntSize.h"
#include "Logging.h"
#include "NotImplemented.h"
#include "SharedBuffer.h"
#include <WinCodec.h>
#include <d2d1.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

ImageDecoderDirect2D::ImageDecoderDirect2D()
{
}

IWICImagingFactory* ImageDecoderDirect2D::systemImagingFactory()
{
    static IWICImagingFactory* wicImagingFactory = nullptr;
    if (!wicImagingFactory) {
        HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory, (LPVOID*)&wicImagingFactory);
        RELEASE_ASSERT(SUCCEEDED(hr));
    }

    return wicImagingFactory;
}

size_t ImageDecoderDirect2D::bytesDecodedToDetermineProperties() const
{
    // Set to match value used for CoreGraphics.
    return 13088;
}

String ImageDecoderDirect2D::filenameExtension() const
{
    notImplemented();
    return String();
}

bool ImageDecoderDirect2D::isSizeAvailable() const
{
    return m_nativeDecoder ? true : false;
}

EncodedDataStatus ImageDecoderDirect2D::encodedDataStatus() const
{
    if (!m_nativeDecoder)
        return EncodedDataStatus::Error;

    // FIXME: Hook into WIC decoder (if async decode is even possible)
    return EncodedDataStatus::Complete;
}

IntSize ImageDecoderDirect2D::size() const
{
    if (!m_nativeDecoder)
        return IntSize();

    COMPtr<IWICBitmapFrameDecode> frame;
    HRESULT hr = m_nativeDecoder->GetFrame(0, &frame);
    if (!SUCCEEDED(hr))
        return IntSize();

    UINT width, height;
    hr = frame->GetSize(&width, &height);
    if (!SUCCEEDED(hr))
        return IntSize();

    return IntSize(width, height);
}

size_t ImageDecoderDirect2D::frameCount() const
{
    if (!m_nativeDecoder)
        return 0;

    UINT count;
    HRESULT hr = m_nativeDecoder->GetFrameCount(&count);
    if (!SUCCEEDED(hr))
        return 0;

    return count;
}

RepetitionCount ImageDecoderDirect2D::repetitionCount() const
{
    if (!m_nativeDecoder)
        return RepetitionCountNone;

    // FIXME: Identify image type, and determine proper repetition count.
    return RepetitionCountNone;
}

std::optional<IntPoint> ImageDecoderDirect2D::hotSpot() const
{
    return IntPoint();
}

IntSize ImageDecoderDirect2D::frameSizeAtIndex(size_t index, SubsamplingLevel subsamplingLevel) const
{
    if (!m_nativeDecoder)
        return IntSize();

    COMPtr<IWICBitmapFrameDecode> frame;
    HRESULT hr = m_nativeDecoder->GetFrame(index, &frame);
    if (!SUCCEEDED(hr))
        return IntSize();

    UINT width, height;
    hr = frame->GetSize(&width, &height);
    if (!SUCCEEDED(hr))
        return IntSize();

    return IntSize(width, height);
}

bool ImageDecoderDirect2D::frameIsCompleteAtIndex(size_t index) const
{
    if (!m_nativeDecoder)
        return false;

    COMPtr<IWICBitmapFrameDecode> frame;
    HRESULT hr = m_nativeDecoder->GetFrame(index, &frame);
    return SUCCEEDED(hr);
}

ImageOrientation ImageDecoderDirect2D::frameOrientationAtIndex(size_t index) const
{
    if (!m_nativeDecoder)
        return ImageOrientation();

    COMPtr<IWICBitmapFrameDecode> frame;
    HRESULT hr = m_nativeDecoder->GetFrame(index, &frame);
    if (!SUCCEEDED(hr))
        return ImageOrientation();

    COMPtr<IWICMetadataQueryReader> metadata;
    hr = frame->GetMetadataQueryReader(&metadata);
    if (!SUCCEEDED(hr))
        return ImageOrientation();

    // FIXME: Identify image type, and ask proper orientation.
    return ImageOrientation();
}

Seconds ImageDecoderDirect2D::frameDurationAtIndex(size_t index) const
{
    if (!m_nativeDecoder)
        return Seconds(0);

    COMPtr<IWICBitmapFrameDecode> frame;
    HRESULT hr = m_nativeDecoder->GetFrame(index, &frame);
    if (!SUCCEEDED(hr))
        return Seconds(0);

    // FIXME: Figure out correct image format-specific query for frame duration check.
    notImplemented();
    return Seconds(0);
}

bool ImageDecoderDirect2D::frameAllowSubsamplingAtIndex(size_t index) const
{
    if (!m_nativeDecoder)
        return false;

    COMPtr<IWICBitmapFrameDecode> frame;
    HRESULT hr = m_nativeDecoder->GetFrame(index, &frame);
    if (!SUCCEEDED(hr))
        return false;

    // FIXME: Figure out correct image format-specific query for subsampling check.
    notImplemented();
    return true;
}

bool ImageDecoderDirect2D::frameHasAlphaAtIndex(size_t index) const
{
    if (!m_nativeDecoder)
        return false;

    COMPtr<IWICBitmapFrameDecode> frame;
    HRESULT hr = m_nativeDecoder->GetFrame(index, &frame);
    if (!SUCCEEDED(hr))
        return false;

    // FIXME: Figure out correct image format-specific query for alpha check.
    notImplemented();
    return true;
}

unsigned ImageDecoderDirect2D::frameBytesAtIndex(size_t index, SubsamplingLevel subsamplingLevel) const
{
    if (!m_nativeDecoder)
        return 0;

    auto frameSize = frameSizeAtIndex(index, subsamplingLevel);
    return (frameSize.area() * 4).unsafeGet();
}

void ImageDecoderDirect2D::setTargetContext(ID2D1RenderTarget* renderTarget)
{
    m_renderTarget = renderTarget;
}

NativeImagePtr ImageDecoderDirect2D::createFrameImageAtIndex(size_t index, SubsamplingLevel subsamplingLevel, const DecodingOptions&)
{
    if (!m_nativeDecoder || !m_renderTarget)
        return nullptr;

    COMPtr<IWICBitmapFrameDecode> frame;
    HRESULT hr = m_nativeDecoder->GetFrame(0, &frame);
    if (!SUCCEEDED(hr))
        return nullptr;

    COMPtr<IWICFormatConverter> converter;
    hr = systemImagingFactory()->CreateFormatConverter(&converter);
    if (!SUCCEEDED(hr))
        return nullptr;

    hr = converter->Initialize(frame.get(), GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0.f, WICBitmapPaletteTypeCustom);
    if (!SUCCEEDED(hr))
        return nullptr;

    COMPtr<ID2D1Bitmap> bitmap;
    hr = m_renderTarget->CreateBitmapFromWicBitmap(converter.get(), nullptr, &bitmap);
    if (!SUCCEEDED(hr))
        return nullptr;

    return bitmap;
}

void ImageDecoderDirect2D::setData(SharedBuffer& data, bool allDataReceived)
{
    if (!allDataReceived)
        return;

    COMPtr<IWICStream> stream;
    HRESULT hr = systemImagingFactory()->CreateStream(&stream);
    if (!SUCCEEDED(hr))
        return;

    char* dataPtr = const_cast<char*>(data.data());
    hr = stream->InitializeFromMemory(reinterpret_cast<BYTE*>(dataPtr), data.size());
    if (!SUCCEEDED(hr))
        return;

    m_nativeDecoder = nullptr;

    hr = systemImagingFactory()->CreateDecoderFromStream(stream.get(), nullptr, WICDecodeMetadataCacheOnDemand, &m_nativeDecoder);
    if (!SUCCEEDED(hr)) {
        m_nativeDecoder = nullptr;
        return;
    }

    // Image was valid.
}
}

#endif
