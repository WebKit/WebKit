/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#if USE(3D_GRAPHICS)

#include "GraphicsContext3D.h"

#include "BitmapImage.h"
#include "Image.h"
#include "ImageDecoder.h"
#include "NativeImageSkia.h"
#include "SkColorPriv.h"
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

#include <algorithm>

namespace WebCore {

GraphicsContext3D::ImageExtractor::~ImageExtractor()
{
    if (m_skiaImage)
        m_skiaImage->bitmap().unlockPixels();
}

bool GraphicsContext3D::ImageExtractor::extractImage(bool premultiplyAlpha, bool ignoreGammaAndColorProfile)
{
    if (!m_image)
        return false;
    m_skiaImage = m_image->nativeImageForCurrentFrame();
    m_alphaOp = AlphaDoNothing;
    bool hasAlpha = m_skiaImage ? !m_skiaImage->bitmap().isOpaque() : true;
    if ((!m_skiaImage || ignoreGammaAndColorProfile || (hasAlpha && !premultiplyAlpha)) && m_image->data()) {
        // Attempt to get raw unpremultiplied image data.
        OwnPtr<ImageDecoder> decoder(adoptPtr(ImageDecoder::create(
            *(m_image->data()), ImageSource::AlphaNotPremultiplied,
            ignoreGammaAndColorProfile ? ImageSource::GammaAndColorProfileIgnored : ImageSource::GammaAndColorProfileApplied)));
        if (!decoder)
            return false;
        decoder->setData(m_image->data(), true);
        if (!decoder->frameCount())
            return false;
        ImageFrame* frame = decoder->frameBufferAtIndex(0);
        if (!frame || frame->status() != ImageFrame::FrameComplete)
            return false;
        hasAlpha = frame->hasAlpha();
        m_nativeImage = adoptPtr(frame->asNewNativeImage());
        if (!m_nativeImage.get() || !m_nativeImage->isDataComplete() || !m_nativeImage->bitmap().width() || !m_nativeImage->bitmap().height())
            return false;
        SkBitmap::Config skiaConfig = m_nativeImage->bitmap().config();
        if (skiaConfig != SkBitmap::kARGB_8888_Config)
            return false;
        m_skiaImage = m_nativeImage.get();
        if (hasAlpha && premultiplyAlpha)
            m_alphaOp = AlphaDoPremultiply;
    } else if (!premultiplyAlpha && hasAlpha) {
        // 1. For texImage2D with HTMLVideoElment input, assume no PremultiplyAlpha had been applied and the alpha value for each pixel is 0xFF
        // which is true at present and may be changed in the future and needs adjustment accordingly.
        // 2. For texImage2D with HTMLCanvasElement input in which Alpha is already Premultiplied in this port,
        // do AlphaDoUnmultiply if UNPACK_PREMULTIPLY_ALPHA_WEBGL is set to false.
        if (m_imageHtmlDomSource != HtmlDomVideo)
            m_alphaOp = AlphaDoUnmultiply;
    }
    if (!m_skiaImage)
        return false;

    m_imageSourceFormat = SK_B32_SHIFT ? SourceFormatRGBA8 : SourceFormatBGRA8;
    m_imageWidth = m_skiaImage->bitmap().width();
    m_imageHeight = m_skiaImage->bitmap().height();
    if (!m_imageWidth || !m_imageHeight)
        return false;
    m_imageSourceUnpackAlignment = 0;
    m_skiaImage->bitmap().lockPixels();
    m_imagePixelData = m_skiaImage->bitmap().getPixels();
    return true;
}

} // namespace WebCore

#endif // USE(3D_GRAPHICS)
