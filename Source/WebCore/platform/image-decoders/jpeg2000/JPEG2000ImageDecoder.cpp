/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "JPEG2000ImageDecoder.h"

#if USE(OPENJPEG)

#include <openjpeg.h>

namespace WebCore {

// SYCC to RGB conversion code from libopenjpeg (BSD), adapted to WebKit coding style.
// --------------------------------------------------------
// Matrix for sYCC, Amendment 1 to IEC 61966-2-1
//
// Y :   0.299   0.587    0.114   :R
// Cb:  -0.1687 -0.3312   0.5     :G
// Cr:   0.5    -0.4187  -0.0812  :B
//
// Inverse:
//
// R: 1        -3.68213e-05    1.40199      :Y
// G: 1.00003  -0.344125      -0.714128     :Cb - 2^(prec - 1)
// B: 0.999823  1.77204       -8.04142e-06  :Cr - 2^(prec - 1)
//
// -----------------------------------------------------------*/
static void syccToRGB(int offset, int upb, int y, int cb, int cr, int* r, int* g, int* b)
{
    cb -= offset;
    cr -= offset;

    *r = static_cast<int>(clampTo<float>(y - 0.0000368 * cb + 1.40199 * cr + 0.5, 0, upb));
    *g = static_cast<int>(clampTo<float>(1.0003 * y - 0.344125 * cb - 0.7141128 * cr + 0.5, 0, upb));
    *b = static_cast<int>(clampTo<float>(0.999823 * y + 1.77204 * cb - 0.000008 * cr + 0.5, 0, upb));
}

static void sycc444ToRGB(opj_image_t* img)
{
    Checked<int> upb = static_cast<int>(img->comps[0].prec);
    int offset = 1 << (upb.unsafeGet() - 1);
    upb = (1 << upb.unsafeGet()) - 1;

    Checked<size_t> maxw = static_cast<size_t>(img->comps[0].w);
    Checked<size_t> maxh = static_cast<size_t>(img->comps[0].h);
    size_t max = (maxw * maxh).unsafeGet();

    const int* y = img->comps[0].data;
    const int* cb = img->comps[1].data;
    const int* cr = img->comps[2].data;

    auto* d0 = static_cast<int*>(opj_image_data_alloc(sizeof(int) * max));
    auto* d1 = static_cast<int*>(opj_image_data_alloc(sizeof(int) * max));
    auto* d2 = static_cast<int*>(opj_image_data_alloc(sizeof(int) * max));
    auto* r = d0;
    auto* g = d1;
    auto* b = d2;

    if (!r || !g || !b) {
        opj_image_data_free(r);
        opj_image_data_free(g);
        opj_image_data_free(b);
        return;
    }

    for (size_t i = 0; i < max; ++i) {
        syccToRGB(offset, upb.unsafeGet(), *y, *cb, *cr, r, g, b);
        ++y;
        ++cb;
        ++cr;
        ++r;
        ++g;
        ++b;
    }

    opj_image_data_free(img->comps[0].data);
    img->comps[0].data = d0;
    opj_image_data_free(img->comps[1].data);
    img->comps[1].data = d1;
    opj_image_data_free(img->comps[2].data);
    img->comps[2].data = d2;
    img->color_space = OPJ_CLRSPC_SRGB;
}

static void sycc422ToRGB(opj_image_t* img)
{
    Checked<int> upb = static_cast<int>(img->comps[0].prec);
    int offset = 1 << (upb.unsafeGet() - 1);
    upb = (1 << upb.unsafeGet()) - 1;

    Checked<size_t> maxw = static_cast<size_t>(img->comps[0].w);
    Checked<size_t> maxh = static_cast<size_t>(img->comps[0].h);
    size_t max = (maxw * maxh).unsafeGet();

    const int* y = img->comps[0].data;
    const int* cb = img->comps[1].data;
    const int* cr = img->comps[2].data;

    auto* d0 = static_cast<int*>(opj_image_data_alloc(sizeof(int) * max));
    auto* d1 = static_cast<int*>(opj_image_data_alloc(sizeof(int) * max));
    auto* d2 = static_cast<int*>(opj_image_data_alloc(sizeof(int) * max));
    auto* r = d0;
    auto* g = d1;
    auto* b = d2;

    if (!r || !g || !b) {
        opj_image_data_free(r);
        opj_image_data_free(g);
        opj_image_data_free(b);
        return;
    }

    // if img->x0 is odd, then first column shall use Cb/Cr = 0.
    size_t offx = img->x0 & 1U;
    size_t loopmaxw = maxw.unsafeGet() - offx;

    for (size_t i = 0; i < maxh.unsafeGet(); ++i) {
        size_t j;

        if (offx > 0) {
            syccToRGB(offset, upb.unsafeGet(), *y, 0, 0, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
        }

        for (j = 0; j < (loopmaxw & ~static_cast<size_t>(1U)); j += 2U) {
            syccToRGB(offset, upb.unsafeGet(), *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            syccToRGB(offset, upb.unsafeGet(), *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            ++cb;
            ++cr;
        }
        if (j < loopmaxw) {
            syccToRGB(offset, upb.unsafeGet(), *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            ++cb;
            ++cr;
        }
    }

    opj_image_data_free(img->comps[0].data);
    img->comps[0].data = d0;
    opj_image_data_free(img->comps[1].data);
    img->comps[1].data = d1;
    opj_image_data_free(img->comps[2].data);
    img->comps[2].data = d2;

    img->comps[1].w = img->comps[2].w = img->comps[0].w;
    img->comps[1].h = img->comps[2].h = img->comps[0].h;
    img->comps[1].dx = img->comps[2].dx = img->comps[0].dx;
    img->comps[1].dy = img->comps[2].dy = img->comps[0].dy;
    img->color_space = OPJ_CLRSPC_SRGB;
}

static void sycc420ToRGB(opj_image_t* img)
{
    Checked<int> upb = static_cast<int>(img->comps[0].prec);
    int offset = 1 << (upb.unsafeGet() - 1);
    upb = (1 << upb.unsafeGet()) - 1;

    Checked<size_t> maxw = static_cast<size_t>(img->comps[0].w);
    Checked<size_t> maxh = static_cast<size_t>(img->comps[0].h);
    size_t max = (maxw * maxh).unsafeGet();

    const int* y = img->comps[0].data;
    const int* cb = img->comps[1].data;
    const int* cr = img->comps[2].data;

    auto* d0 = static_cast<int*>(opj_image_data_alloc(sizeof(int) * max));
    auto* d1 = static_cast<int*>(opj_image_data_alloc(sizeof(int) * max));
    auto* d2 = static_cast<int*>(opj_image_data_alloc(sizeof(int) * max));
    auto* r = d0;
    auto* g = d1;
    auto* b = d2;

    if (!r || !g || !b) {
        opj_image_data_free(r);
        opj_image_data_free(g);
        opj_image_data_free(b);
        return;
    }

    // if img->x0 is odd, then first column shall use Cb/Cr = 0.
    size_t offx = img->x0 & 1U;
    size_t loopmaxw = maxw.unsafeGet() - offx;
    // if img->y0 is odd, then first line shall use Cb/Cr = 0.
    size_t offy = img->y0 & 1U;
    size_t loopmaxh = maxh.unsafeGet() - offy;

    if (offy > 0) {
        size_t j;
        for (j = 0; j < maxw.unsafeGet(); ++j) {
            syccToRGB(offset, upb.unsafeGet(), *y, 0, 0, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
        }
    }

    size_t i;
    for (i = 0; i < (loopmaxh & ~static_cast<size_t>(1U)); i += 2U) {
        const int* ny = y + maxw.unsafeGet();
        int* nr = r + maxw.unsafeGet();
        int* ng = g + maxw.unsafeGet();
        int* nb = b + maxw.unsafeGet();

        if (offx > 0) {
            syccToRGB(offset, upb.unsafeGet(), *y, 0, 0, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            syccToRGB(offset, upb.unsafeGet(), *ny, *cb, *cr, nr, ng, nb);
            ++ny;
            ++nr;
            ++ng;
            ++nb;
        }

        size_t j;
        for (j = 0; j < (loopmaxw & ~static_cast<size_t>(1U)); j += 2U) {
            syccToRGB(offset, upb.unsafeGet(), *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;
            syccToRGB(offset, upb.unsafeGet(), *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;

            syccToRGB(offset, upb.unsafeGet(), *ny, *cb, *cr, nr, ng, nb);
            ++ny;
            ++nr;
            ++ng;
            ++nb;
            syccToRGB(offset, upb.unsafeGet(), *ny, *cb, *cr, nr, ng, nb);
            ++ny;
            ++nr;
            ++ng;
            ++nb;
            ++cb;
            ++cr;
        }
        if (j < loopmaxw) {
            syccToRGB(offset, upb.unsafeGet(), *y, *cb, *cr, r, g, b);
            ++y;
            ++r;
            ++g;
            ++b;

            syccToRGB(offset, upb.unsafeGet(), *ny, *cb, *cr, nr, ng, nb);
            ++ny;
            ++nr;
            ++ng;
            ++nb;
            ++cb;
            ++cr;
        }
        y += maxw.unsafeGet();
        r += maxw.unsafeGet();
        g += maxw.unsafeGet();
        b += maxw.unsafeGet();
    }
    if (i < loopmaxh) {
        size_t j;
        for (j = 0; j < (maxw.unsafeGet() & ~static_cast<size_t>(1U)); j += 2U) {
            syccToRGB(offset, upb.unsafeGet(), *y, *cb, *cr, r, g, b);

            ++y;
            ++r;
            ++g;
            ++b;

            syccToRGB(offset, upb.unsafeGet(), *y, *cb, *cr, r, g, b);

            ++y;
            ++r;
            ++g;
            ++b;
            ++cb;
            ++cr;
        }
        if (j < maxw.unsafeGet())
            syccToRGB(offset, upb.unsafeGet(), *y, *cb, *cr, r, g, b);
    }

    opj_image_data_free(img->comps[0].data);
    img->comps[0].data = d0;
    opj_image_data_free(img->comps[1].data);
    img->comps[1].data = d1;
    opj_image_data_free(img->comps[2].data);
    img->comps[2].data = d2;

    img->comps[1].w = img->comps[2].w = img->comps[0].w;
    img->comps[1].h = img->comps[2].h = img->comps[0].h;
    img->comps[1].dx = img->comps[2].dx = img->comps[0].dx;
    img->comps[1].dy = img->comps[2].dy = img->comps[0].dy;
    img->color_space = OPJ_CLRSPC_SRGB;
}

JPEG2000ImageDecoder::JPEG2000ImageDecoder(Format format, AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : ScalableImageDecoder(alphaOption, gammaAndColorProfileOption)
    , m_format(format)
{
}

ScalableImageDecoderFrame* JPEG2000ImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index)
        return nullptr;

    if (m_frameBufferCache.isEmpty())
        m_frameBufferCache.grow(1);

    auto& frame = m_frameBufferCache[0];
    if (!frame.isComplete())
        decode(false, isAllDataReceived());
    return &frame;
}

void JPEG2000ImageDecoder::decode(bool onlySize, bool allDataReceived)
{
    if (failed())
        return;

    std::unique_ptr<opj_codec_t, void(*)(opj_codec_t*)> decoder(opj_create_decompress(m_format == Format::JP2 ? OPJ_CODEC_JP2 : OPJ_CODEC_J2K), opj_destroy_codec);
    if (!decoder) {
        setFailed();
        return;
    }

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);
    if (!opj_setup_decoder(decoder.get(), &parameters)) {
        setFailed();
        return;
    }

    std::unique_ptr<opj_stream_t, void(*)(opj_stream_t*)> stream(opj_stream_default_create(OPJ_TRUE), opj_stream_destroy);
    if (!stream) {
        setFailed();
        return;
    }

    struct Reader {
        SharedBuffer& data;
        size_t offset;
    } reader = { *m_data, 0 };

    opj_stream_set_user_data(stream.get(), &reader, nullptr);
    opj_stream_set_user_data_length(stream.get(), m_data->size());
    opj_stream_set_read_function(stream.get(), [](void* buffer, OPJ_SIZE_T bytes, void* userData) -> OPJ_SIZE_T {
        auto& reader = *static_cast<Reader*>(userData);
        if (reader.offset == reader.data.size())
            return -1;

        OPJ_SIZE_T length = reader.offset + bytes > reader.data.size() ? reader.data.size() - reader.offset : bytes;
        memcpy(buffer, reader.data.data(), length);
        reader.offset += length;

        return length;
    });
    opj_stream_set_skip_function(stream.get(), [](OPJ_OFF_T bytes, void* userData) -> OPJ_OFF_T {
        auto& reader = *static_cast<Reader*>(userData);

        OPJ_OFF_T skip = reader.offset + bytes > reader.data.size() ? reader.data.size() - reader.offset : bytes;
        reader.offset += skip;

        return skip;
    });
    opj_stream_set_seek_function(stream.get(), [](OPJ_OFF_T bytes, void* userData) -> OPJ_BOOL {
        auto& reader = *static_cast<Reader*>(userData);

        if (static_cast<unsigned>(bytes) > reader.data.size())
            return OPJ_FALSE;

        reader.offset = bytes;

        return OPJ_TRUE;
    });

    opj_image_t* imagePtr = nullptr;
    if (!opj_read_header(stream.get(), decoder.get(), &imagePtr)) {
        if (allDataReceived)
            setFailed();
        opj_image_destroy(imagePtr);
        return;
    }

    std::unique_ptr<opj_image_t, void(*)(opj_image_t*)> image(imagePtr, opj_image_destroy);
    setSize({ static_cast<int>(image->x1 - image->x0), static_cast<int>(image->y1 - image->y0) });
    if (onlySize)
        return;

    if (!opj_decode(decoder.get(), stream.get(), image.get())) {
        if (allDataReceived)
            setFailed();
        return;
    }

    if (image->color_space == OPJ_CLRSPC_UNSPECIFIED) {
        if (image->numcomps == 3 && image->comps[0].dx == image->comps[0].dy && image->comps[1].dx != 1)
            image->color_space = OPJ_CLRSPC_SYCC;
        else if (image->numcomps <= 2)
            image->color_space = OPJ_CLRSPC_GRAY;
    }

    if (image->color_space == OPJ_CLRSPC_SYCC) {
        if (image->numcomps < 3)
            image->color_space = OPJ_CLRSPC_GRAY;
        else if ((image->comps[0].dx == 1)
            && (image->comps[1].dx == 2)
            && (image->comps[2].dx == 2)
            && (image->comps[0].dy == 1)
            && (image->comps[1].dy == 2)
            && (image->comps[2].dy == 2)) {
            // Horizontal and vertical sub-sample.
            sycc420ToRGB(image.get());
        } else if ((image->comps[0].dx == 1)
            && (image->comps[1].dx == 2)
            && (image->comps[2].dx == 2)
            && (image->comps[0].dy == 1)
            && (image->comps[1].dy == 1)
            && (image->comps[2].dy == 1)) {
            // Horizontal sub-sample only.
            sycc422ToRGB(image.get());
        } else if ((image->comps[0].dx == 1)
            && (image->comps[1].dx == 1)
            && (image->comps[2].dx == 1)
            && (image->comps[0].dy == 1)
            && (image->comps[1].dy == 1)
            && (image->comps[2].dy == 1)) {
            // No sub-sample.
            sycc444ToRGB(image.get());
        }
    }

    if (image->color_space != OPJ_CLRSPC_SRGB || image->numcomps > 4 || image->numcomps < 3) {
        // Unsupported format.
        setFailed();
        return;
    }

    auto& buffer = m_frameBufferCache[0];
    if (!buffer.initialize(scaledSize(), m_premultiplyAlpha)) {
        setFailed();
        return;
    }

    buffer.setDecodingStatus(DecodingStatus::Partial);
    buffer.setHasAlpha(false);

    int adjust[4] = { 0, 0, 0, 0 };
    for (OPJ_UINT32 component = 0; component < image->numcomps; ++component) {
        if (!image->comps[component].data) {
            setFailed();
            break;
        }

        if (image->comps[component].prec > 8)
            adjust[component] = image->comps[component].prec - 8;
    }

    bool subsampling = image->comps[0].dx != 1 || image->comps[0].dy != 1 || image->comps[1].dx != 1 || image->comps[1].dy != 1 || image->comps[2].dx != 1 || image->comps[2].dy != 1;
    unsigned char nonTrivialAlphaMask = 0;
    const IntRect& frameRect = buffer.backingStore()->frameRect();
    for (int y = 0; y < frameRect.height(); ++y) {
        for (int x = 0; x < frameRect.width(); ++x) {
            int r, g, b, a;

            int offset;
            if (subsampling) {
                int subX = frameRect.width() / image->comps[0].w;
                int subY = frameRect.height() / image->comps[0].h;
                offset = (y / subY) * image->comps[0].w + (x / subX);
            } else
                offset = y * frameRect.width() + x;

            r = image->comps[0].data[offset];
            r += (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);
            if (subsampling) {
                int subX = frameRect.width() / image->comps[1].w;
                int subY = frameRect.height() / image->comps[1].h;
                offset = (y / subY) * image->comps[1].w + (x / subX);
            }
            g = image->comps[1].data[offset];
            g += (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);
            if (subsampling) {
                int subX = frameRect.width() / image->comps[2].w;
                int subY = frameRect.height() / image->comps[2].h;
                offset = (y / subY) * image->comps[2].w + (x / subX);
            }
            b = image->comps[2].data[offset];
            b += (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);

            if (image->numcomps > 3) {
                if (subsampling) {
                    int subX = frameRect.width() / image->comps[3].w;
                    int subY = frameRect.height() / image->comps[3].h;
                    offset = (y / subY) * image->comps[3].w + (x / subX);
                }
                a = image->comps[3].data[offset];
                a += (image->comps[3].sgnd ? 1 << (image->comps[3].prec - 1) : 0);
            }

            int adjustedRed = (r >> adjust[0]) + ((r >> (adjust[0] - 1)) % 2);
            int adjustedGreen = (g >> adjust[1]) + ((g >> (adjust[1] - 1)) % 2);
            int adjustedBlue = (b >> adjust[2]) + ((b >> (adjust[2] - 1)) % 2);
            int adjustedAlpha = image->numcomps > 3 ? (a >> adjust[3]) + ((a >> (adjust[3] - 1)) % 2) : 0xFF;
            buffer.backingStore()->setPixel(x, y, adjustedRed, adjustedGreen, adjustedBlue, adjustedAlpha);
            nonTrivialAlphaMask |= (255 - adjustedAlpha);
        }
    }

    buffer.setDecodingStatus(DecodingStatus::Complete);
    if (nonTrivialAlphaMask && !buffer.hasAlpha())
        buffer.setHasAlpha(true);
}

} // namespace WebCore

#endif // USE(OPENJPEG)
