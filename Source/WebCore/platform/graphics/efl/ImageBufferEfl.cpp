/*
 * Copyright (C) 2015 Samsung Electronics. All rights reserved.
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
#include "ImageBuffer.h"

#include "MIMETypeRegistry.h"
#include "image-encoders/JPEGImageEncoder.h"

#include <cairo.h>
#include <wtf/text/Base64.h>


namespace WebCore {

static cairo_status_t writeFunction(void* output, const unsigned char* data, unsigned length)
{
    if (!reinterpret_cast<Vector<unsigned char>*>(output)->tryAppend(data, length))
        return CAIRO_STATUS_WRITE_ERROR;
    return CAIRO_STATUS_SUCCESS;
}

static bool encodeImagePNG(cairo_surface_t* image, Vector<char>* output)
{
    return cairo_surface_write_to_png_stream(image, writeFunction, output) == CAIRO_STATUS_SUCCESS;
}

static bool encodeImageJPEG(unsigned char* data, IntSize size, Vector<char>* output, std::optional<double> quality)
{    
    return compressRGBABigEndianToJPEG(data, size, *output, quality);
}

String ImageBuffer::toDataURL(const String& mimeType, std::optional<double> quality, CoordinateSystem) const
{
    ASSERT(MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(mimeType));

    cairo_surface_t* image = cairo_get_target(context().platformContext()->cr());

    Vector<char> encodedImage;

    if (!image)
        return "data:,";

    if (mimeType == "image/png") {
        if (!encodeImagePNG(image, &encodedImage))
            return "data:,";
    }

    if (mimeType == "image/jpeg") {
        int width = cairo_image_surface_get_width(image);
        int height = cairo_image_surface_get_height(image);

        IntSize size(width, height);
        IntRect dataRect(IntPoint(), size);
        RefPtr<Uint8ClampedArray> myData = getPremultipliedImageData(dataRect);

        if (!encodeImageJPEG(myData->data(), size, &encodedImage, quality))
            return "data:,";
    }

    Vector<char> base64Data;
    base64Encode(encodedImage, base64Data);

    return "data:" + mimeType + ";base64," + base64Data;
}

}
