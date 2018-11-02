/*
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 *
 * Portions are Copyright (C) 2001 mozilla.org
 *
 * Other contributors:
 *   Stuart Parmenter <stuart@mozilla.com>
 *   Max Stepin <maxstepin@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "PNGImageDecoder.h"

#include "Color.h"
#include <png.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniqueArray.h>

#if defined(PNG_LIBPNG_VER_MAJOR) && defined(PNG_LIBPNG_VER_MINOR) && (PNG_LIBPNG_VER_MAJOR > 1 || (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR >= 4))
#define JMPBUF(png_ptr) png_jmpbuf(png_ptr)
#else
#define JMPBUF(png_ptr) png_ptr->jmpbuf
#endif

namespace WebCore {

// Gamma constants.
const double cMaxGamma = 21474.83;
const double cDefaultGamma = 2.2;
const double cInverseGamma = 0.45455;

// Protect against large PNGs. See Mozilla's bug #251381 for more info.
const unsigned long cMaxPNGSize = 1000000UL;

// Called if the decoding of the image fails.
static void PNGAPI decodingFailed(png_structp png, png_const_charp)
{
    longjmp(JMPBUF(png), 1);
}

// Callbacks given to the read struct.  The first is for warnings (we want to
// treat a particular warning as an error, which is why we have to register this
// callback).
static void PNGAPI decodingWarning(png_structp png, png_const_charp warningMsg)
{
    // Mozilla did this, so we will too.
    // Convert a tRNS warning to be an error (see
    // http://bugzilla.mozilla.org/show_bug.cgi?id=251381 )
    if (!strncmp(warningMsg, "Missing PLTE before tRNS", 24))
        png_error(png, warningMsg);
}

// Called when we have obtained the header information (including the size).
static void PNGAPI headerAvailable(png_structp png, png_infop)
{
    static_cast<PNGImageDecoder*>(png_get_progressive_ptr(png))->headerAvailable();
}

// Called when a row is ready.
static void PNGAPI rowAvailable(png_structp png, png_bytep rowBuffer, png_uint_32 rowIndex, int interlacePass)
{
    static_cast<PNGImageDecoder*>(png_get_progressive_ptr(png))->rowAvailable(rowBuffer, rowIndex, interlacePass);
}

// Called when we have completely finished decoding the image.
static void PNGAPI pngComplete(png_structp png, png_infop)
{
    static_cast<PNGImageDecoder*>(png_get_progressive_ptr(png))->pngComplete();
}

#if ENABLE(APNG)
// Called when we have the frame header.
static void PNGAPI frameHeader(png_structp png, png_infop)
{
    static_cast<PNGImageDecoder*>(png_get_progressive_ptr(png))->frameHeader();
}

// Called when we found user chunks.
static int PNGAPI readChunks(png_structp png, png_unknown_chunkp chunk)
{
    static_cast<PNGImageDecoder*>(png_get_user_chunk_ptr(png))->readChunks(chunk);
    return 1;
}
#endif

class PNGImageReader {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PNGImageReader(PNGImageDecoder* decoder)
        : m_readOffset(0)
        , m_currentBufferSize(0)
        , m_decodingSizeOnly(false)
        , m_hasAlpha(false)
    {
        m_png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, decodingFailed, decodingWarning);
        m_info = png_create_info_struct(m_png);
        png_set_progressive_read_fn(m_png, decoder, headerAvailable, rowAvailable, pngComplete);
#if ENABLE(APNG)
        png_byte apngChunks[]= {"acTL\0fcTL\0fdAT\0"};
        png_set_keep_unknown_chunks(m_png, 1, apngChunks, 3);
        png_set_read_user_chunk_fn(m_png, static_cast<png_voidp>(decoder), readChunks);
        decoder->init();
#endif
    }

    ~PNGImageReader()
    {
        close();
    }

    void close()
    {
        if (m_png && m_info)
            // This will zero the pointers.
            png_destroy_read_struct(&m_png, &m_info, 0);
        m_interlaceBuffer.reset();
        m_readOffset = 0;
    }

    bool decode(const SharedBuffer& data, bool sizeOnly, unsigned haltAtFrame)
    {
        m_decodingSizeOnly = sizeOnly;
        PNGImageDecoder* decoder = static_cast<PNGImageDecoder*>(png_get_progressive_ptr(m_png));

        // We need to do the setjmp here. Otherwise bad things will happen.
        if (setjmp(JMPBUF(m_png)))
            return decoder->setFailed();

        auto bytesToSkip = m_readOffset;
        
        // FIXME: Use getSomeData which is O(log(n)) instead of skipping bytes which is O(n).
        for (const auto& element : data) {
            if (bytesToSkip > element.segment->size()) {
                bytesToSkip -= element.segment->size();
                continue;
            }
            auto bytesToUse = element.segment->size() - bytesToSkip;
            m_readOffset += bytesToUse;
            m_currentBufferSize = m_readOffset;
            png_process_data(m_png, m_info, reinterpret_cast<png_bytep>(const_cast<char*>(element.segment->data() + bytesToSkip)), bytesToUse);
            bytesToSkip = 0;
            // We explicitly specify the superclass encodedDataStatus() because we
            // merely want to check if we've managed to set the size, not
            // (recursively) trigger additional decoding if we haven't.
            if (sizeOnly ? decoder->ScalableImageDecoder::encodedDataStatus() >= EncodedDataStatus::SizeAvailable : decoder->isCompleteAtIndex(haltAtFrame))
                return true;
        }
        return false;
    }

    png_structp pngPtr() const { return m_png; }
    png_infop infoPtr() const { return m_info; }

    void setReadOffset(unsigned offset) { m_readOffset = offset; }
    unsigned currentBufferSize() const { return m_currentBufferSize; }
    bool decodingSizeOnly() const { return m_decodingSizeOnly; }
    void setHasAlpha(bool hasAlpha) { m_hasAlpha = hasAlpha; }
    bool hasAlpha() const { return m_hasAlpha; }

    png_bytep interlaceBuffer() const { return m_interlaceBuffer.get(); }
    void createInterlaceBuffer(int size) { m_interlaceBuffer = makeUniqueArray<png_byte>(size); }

private:
    png_structp m_png;
    png_infop m_info;
    unsigned m_readOffset;
    unsigned m_currentBufferSize;
    bool m_decodingSizeOnly;
    bool m_hasAlpha;
    UniqueArray<png_byte> m_interlaceBuffer;
};

PNGImageDecoder::PNGImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : ScalableImageDecoder(alphaOption, gammaAndColorProfileOption)
    , m_doNothingOnFailure(false)
    , m_currentFrame(0)
#if ENABLE(APNG)
    , m_png(nullptr)
    , m_info(nullptr)
    , m_isAnimated(false)
    , m_frameInfo(false)
    , m_frameIsHidden(false)
    , m_hasInfo(false)
    , m_gamma(45455)
    , m_frameCount(1)
    , m_playCount(0)
    , m_totalFrames(0)
    , m_sizePLTE(0)
    , m_sizetRNS(0)
    , m_sequenceNumber(0)
    , m_width(0)
    , m_height(0)
    , m_xOffset(0)
    , m_yOffset(0)
    , m_delayNumerator(1)
    , m_delayDenominator(1)
    , m_dispose(0)
    , m_blend(0)
#endif
{
}

PNGImageDecoder::~PNGImageDecoder() = default;

#if ENABLE(APNG)
RepetitionCount PNGImageDecoder::repetitionCount() const
{
    // Signal no repetition if the PNG image is not animated.
    if (!m_isAnimated)
        return RepetitionCountNone;

    // APNG format uses 0 to indicate that an animation must play indefinitely. But
    // the RepetitionCount enumeration uses RepetitionCountInfinite, so we need to adapt this.
    if (!m_playCount)
        return RepetitionCountInfinite;

    return m_playCount;
}
#endif

bool PNGImageDecoder::setSize(const IntSize& size)
{
    if (!ScalableImageDecoder::setSize(size))
        return false;

    prepareScaleDataIfNecessary();
    return true;
}

ScalableImageDecoderFrame* PNGImageDecoder::frameBufferAtIndex(size_t index)
{
#if ENABLE(APNG)
    if (ScalableImageDecoder::encodedDataStatus() < EncodedDataStatus::SizeAvailable)
        return nullptr;

    if (index >= frameCount())
        index = frameCount() - 1;
#else
    if (index)
        return nullptr;
#endif

    if (m_frameBufferCache.isEmpty())
        m_frameBufferCache.grow(1);

    auto& frame = m_frameBufferCache[index];
    if (!frame.isComplete())
        decode(false, index, isAllDataReceived());
    return &frame;
}

bool PNGImageDecoder::setFailed()
{
    if (m_doNothingOnFailure)
        return false;
    m_reader = nullptr;
    return ScalableImageDecoder::setFailed();
}

void PNGImageDecoder::headerAvailable()
{
    png_structp png = m_reader->pngPtr();
    png_infop info = m_reader->infoPtr();
    png_uint_32 width = png_get_image_width(png, info);
    png_uint_32 height = png_get_image_height(png, info);

    // Protect against large images.
    if (width > cMaxPNGSize || height > cMaxPNGSize) {
        longjmp(JMPBUF(png), 1);
        return;
    }

    // We can fill in the size now that the header is available.  Avoid memory
    // corruption issues by neutering setFailed() during this call; if we don't
    // do this, failures will cause |m_reader| to be deleted, and our jmpbuf
    // will cease to exist.  Note that we'll still properly set the failure flag
    // in this case as soon as we longjmp().
    m_doNothingOnFailure = true;
    bool result = setSize(IntSize(width, height));
    m_doNothingOnFailure = false;
    if (!result) {
        longjmp(JMPBUF(png), 1);
        return;
    }

    int bitDepth, colorType, interlaceType, compressionType, filterType, channels;
    png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType, &interlaceType, &compressionType, &filterType);

    // The options we set here match what Mozilla does.

#if ENABLE(APNG)
    m_hasInfo = true;
    if (m_isAnimated) {
        png_save_uint_32(m_dataIHDR, 13);
        memcpy(m_dataIHDR + 4, "IHDR", 4);
        png_save_uint_32(m_dataIHDR + 8, width);
        png_save_uint_32(m_dataIHDR + 12, height);
        m_dataIHDR[16] = bitDepth;
        m_dataIHDR[17] = colorType;
        m_dataIHDR[18] = compressionType;
        m_dataIHDR[19] = filterType;
        m_dataIHDR[20] = interlaceType;
    }
#endif

    // Expand to ensure we use 24-bit for RGB and 32-bit for RGBA.
    if (colorType == PNG_COLOR_TYPE_PALETTE) {
#if ENABLE(APNG)
        if (m_isAnimated) {
            png_colorp palette;
            int paletteSize = 0;
            png_get_PLTE(png, info, &palette, &paletteSize);
            paletteSize *= 3;
            png_save_uint_32(m_dataPLTE, paletteSize);
            memcpy(m_dataPLTE + 4, "PLTE", 4);
            memcpy(m_dataPLTE + 8, palette, paletteSize);
            m_sizePLTE = paletteSize + 12;
        }
#endif
        png_set_expand(png);
    }

    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8)
        png_set_expand(png);

    png_bytep trns = 0;
    int trnsCount = 0;
    png_color_16p transValues;
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_get_tRNS(png, info, &trns, &trnsCount, &transValues);
#if ENABLE(APNG)
        if (m_isAnimated) {
            if (colorType == PNG_COLOR_TYPE_RGB) {
                png_save_uint_16(m_datatRNS + 8, transValues->red);
                png_save_uint_16(m_datatRNS + 10, transValues->green);
                png_save_uint_16(m_datatRNS + 12, transValues->blue);
                trnsCount = 6;
            } else if (colorType == PNG_COLOR_TYPE_GRAY) {
                png_save_uint_16(m_datatRNS + 8, transValues->gray);
                trnsCount = 2;
            } else if (colorType == PNG_COLOR_TYPE_PALETTE)
                memcpy(m_datatRNS + 8, trns, trnsCount);

            png_save_uint_32(m_datatRNS, trnsCount);
            memcpy(m_datatRNS + 4, "tRNS", 4);
            m_sizetRNS = trnsCount + 12;
        }
#endif
        png_set_expand(png);
    }

    if (bitDepth == 16)
        png_set_strip_16(png);

    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    // Deal with gamma and keep it under our control.
    double gamma;
    if (!m_ignoreGammaAndColorProfile && png_get_gAMA(png, info, &gamma)) {
        if ((gamma <= 0.0) || (gamma > cMaxGamma)) {
            gamma = cInverseGamma;
            png_set_gAMA(png, info, gamma);
        }
        png_set_gamma(png, cDefaultGamma, gamma);
#if ENABLE(APNG)
        m_gamma = static_cast<int>(gamma * 100000);
#endif
    } else
        png_set_gamma(png, cDefaultGamma, cInverseGamma);

    // Tell libpng to send us rows for interlaced pngs.
    if (interlaceType == PNG_INTERLACE_ADAM7)
        png_set_interlace_handling(png);

    // Update our info now.
    png_read_update_info(png, info);
    channels = png_get_channels(png, info);
    ASSERT(channels == 3 || channels == 4);

    m_reader->setHasAlpha(channels == 4);

    if (m_reader->decodingSizeOnly()) {
        // If we only needed the size, halt the reader.
#if defined(PNG_LIBPNG_VER_MAJOR) && defined(PNG_LIBPNG_VER_MINOR) && (PNG_LIBPNG_VER_MAJOR > 1 || (PNG_LIBPNG_VER_MAJOR == 1 && PNG_LIBPNG_VER_MINOR >= 5))
        // '0' argument to png_process_data_pause means: Do not cache unprocessed data.
        m_reader->setReadOffset(m_reader->currentBufferSize() - png_process_data_pause(png, 0));
#else
        m_reader->setReadOffset(m_reader->currentBufferSize() - png->buffer_size);
        png->buffer_size = 0;
#endif
    }
}

void PNGImageDecoder::rowAvailable(unsigned char* rowBuffer, unsigned rowIndex, int)
{
    if (m_frameBufferCache.isEmpty())
        return;

    // Initialize the framebuffer if needed.
#if ENABLE(APNG)
    if (m_currentFrame >= frameCount())
        return;
#endif
    auto& buffer = m_frameBufferCache[m_currentFrame];
    if (buffer.isInvalid()) {
        png_structp png = m_reader->pngPtr();
        if (!buffer.initialize(scaledSize(), m_premultiplyAlpha)) {
            longjmp(JMPBUF(png), 1);
            return;
        }

        unsigned colorChannels = m_reader->hasAlpha() ? 4 : 3;
        if (PNG_INTERLACE_ADAM7 == png_get_interlace_type(png, m_reader->infoPtr())
            || m_currentFrame) {
            if (!m_reader->interlaceBuffer())
                m_reader->createInterlaceBuffer(colorChannels * size().width() * size().height());
            if (!m_reader->interlaceBuffer()) {
                longjmp(JMPBUF(png), 1);
                return;
            }
        }

        buffer.setDecodingStatus(DecodingStatus::Partial);
        buffer.setHasAlpha(false);

#if ENABLE(APNG)
        if (m_currentFrame)
            initFrameBuffer(m_currentFrame);
#endif
    }

    /* libpng comments (here to explain what follows).
     *
     * this function is called for every row in the image.  If the
     * image is interlacing, and you turned on the interlace handler,
     * this function will be called for every row in every pass.
     * Some of these rows will not be changed from the previous pass.
     * When the row is not changed, the new_row variable will be NULL.
     * The rows and passes are called in order, so you don't really
     * need the row_num and pass, but I'm supplying them because it
     * may make your life easier.
     */

    // Nothing to do if the row is unchanged, or the row is outside
    // the image bounds: libpng may send extra rows, ignore them to
    // make our lives easier.
    if (!rowBuffer)
        return;
    int y = !m_scaled ? rowIndex : scaledY(rowIndex);
    if (y < 0 || y >= scaledSize().height())
        return;

    /* libpng comments (continued).
     *
     * For the non-NULL rows of interlaced images, you must call
     * png_progressive_combine_row() passing in the row and the
     * old row.  You can call this function for NULL rows (it will
     * just return) and for non-interlaced images (it just does the
     * memcpy for you) if it will make the code easier.  Thus, you
     * can just do this for all cases:
     *
     *    png_progressive_combine_row(png_ptr, old_row, new_row);
     *
     * where old_row is what was displayed for previous rows.  Note
     * that the first pass (pass == 0 really) will completely cover
     * the old row, so the rows do not have to be initialized.  After
     * the first pass (and only for interlaced images), you will have
     * to pass the current row, and the function will combine the
     * old row and the new row.
     */

    bool hasAlpha = m_reader->hasAlpha();
    unsigned colorChannels = hasAlpha ? 4 : 3;
    png_bytep row = rowBuffer;

    if (png_bytep interlaceBuffer = m_reader->interlaceBuffer()) {
        row = interlaceBuffer + (rowIndex * colorChannels * size().width());
#if ENABLE(APNG)
        if (m_currentFrame) {
            png_progressive_combine_row(m_png, row, rowBuffer);
            return; // Only do incremental image display for the first frame.
        }
#endif
        png_progressive_combine_row(m_reader->pngPtr(), row, rowBuffer);
    }

    // Write the decoded row pixels to the frame buffer.
    auto* address = buffer.backingStore()->pixelAt(0, y);
    int width = scaledSize().width();
    unsigned char nonTrivialAlphaMask = 0;

    png_bytep pixel = row;
    if (hasAlpha) {
        for (int x = 0; x < width; ++x, pixel += 4, ++address) {
            unsigned alpha = pixel[3];
            buffer.backingStore()->setPixel(address, pixel[0], pixel[1], pixel[2], alpha);
            nonTrivialAlphaMask |= (255 - alpha);
        }
    } else {
        for (int x = 0; x < width; ++x, pixel += 3, ++address)
            *address = makeRGB(pixel[0], pixel[1], pixel[2]);
    }

    if (nonTrivialAlphaMask && !buffer.hasAlpha())
        buffer.setHasAlpha(true);
}

void PNGImageDecoder::pngComplete()
{
#if ENABLE(APNG)
    if (m_isAnimated) {
        if (!processingFinish() && m_frameCount == m_currentFrame)
            return;

        fallbackNotAnimated();
    }
#endif
    if (!m_frameBufferCache.isEmpty())
        m_frameBufferCache.first().setDecodingStatus(DecodingStatus::Complete);
}

void PNGImageDecoder::decode(bool onlySize, unsigned haltAtFrame, bool allDataReceived)
{
    if (failed())
        return;

    if (!m_reader)
        m_reader = std::make_unique<PNGImageReader>(this);

    // If we couldn't decode the image but we've received all the data, decoding
    // has failed.
    if (!m_reader->decode(*m_data, onlySize, haltAtFrame) && allDataReceived)
        setFailed();
    // If we're done decoding the image, we don't need the PNGImageReader
    // anymore.  (If we failed, |m_reader| has already been cleared.)
    else if (isComplete())
        m_reader = nullptr;
}

#if ENABLE(APNG)
void PNGImageDecoder::readChunks(png_unknown_chunkp chunk)
{
    if (!memcmp(chunk->name, "acTL", 4) && chunk->size == 8) {
        if (m_hasInfo || m_isAnimated)
            return;

        m_frameCount = png_get_uint_32(chunk->data);
        m_playCount = png_get_uint_32(chunk->data + 4);

        if (!m_frameCount || m_frameCount > PNG_UINT_31_MAX || m_playCount > PNG_UINT_31_MAX) {
            fallbackNotAnimated();
            return;
        }

        m_isAnimated = true;
        if (!m_frameInfo)
            m_frameIsHidden = true;

        if (m_frameBufferCache.size() == m_frameCount)
            return;

        m_frameBufferCache.resize(m_frameCount);
    } else if (!memcmp(chunk->name, "fcTL", 4) && chunk->size == 26) {
        if (m_hasInfo && !m_isAnimated)
            return;

        m_frameInfo = false;

        if (processingFinish()) {
            fallbackNotAnimated();
            return;
        }

        // At this point the old frame is done. Let's start a new one.
        unsigned sequenceNumber = png_get_uint_32(chunk->data);
        if (sequenceNumber != m_sequenceNumber++) {
            fallbackNotAnimated();
            return;
        }

        m_width = png_get_uint_32(chunk->data + 4);
        m_height = png_get_uint_32(chunk->data + 8);
        m_xOffset = png_get_uint_32(chunk->data + 12);
        m_yOffset = png_get_uint_32(chunk->data + 16);
        m_delayNumerator = png_get_uint_16(chunk->data + 20);
        m_delayDenominator = png_get_uint_16(chunk->data + 22);
        m_dispose = chunk->data[24];
        m_blend = chunk->data[25];

        png_structp png = m_reader->pngPtr();
        png_infop info = m_reader->infoPtr();
        png_uint_32 width = png_get_image_width(png, info);
        png_uint_32 height = png_get_image_height(png, info);

        if (m_width > cMaxPNGSize || m_height > cMaxPNGSize
            || m_xOffset > cMaxPNGSize || m_yOffset > cMaxPNGSize
            || m_xOffset + m_width > width
            || m_yOffset + m_height > height
            || m_dispose > 2 || m_blend > 1) {
            fallbackNotAnimated();
            return;
        }

        if (m_frameBufferCache.isEmpty())
            m_frameBufferCache.grow(1);

        if (m_currentFrame < m_frameBufferCache.size()) {
            auto& buffer = m_frameBufferCache[m_currentFrame];

            if (!m_delayDenominator)
                buffer.setDuration(Seconds::fromMilliseconds(m_delayNumerator * 10));
            else
                buffer.setDuration(Seconds::fromMilliseconds(m_delayNumerator * 1000 / m_delayDenominator));

            if (m_dispose == 2)
                buffer.setDisposalMethod(ScalableImageDecoderFrame::DisposalMethod::RestoreToPrevious);
            else if (m_dispose == 1)
                buffer.setDisposalMethod(ScalableImageDecoderFrame::DisposalMethod::RestoreToBackground);
            else
                buffer.setDisposalMethod(ScalableImageDecoderFrame::DisposalMethod::DoNotDispose);
        }

        m_frameInfo = true;
        m_frameIsHidden = false;

        if (processingStart(chunk)) {
            fallbackNotAnimated();
            return;
        }
    } else if (!memcmp(chunk->name, "fdAT", 4) && chunk->size >= 4) {
        if (!m_frameInfo || !m_isAnimated)
            return;

        unsigned sequenceNumber = png_get_uint_32(chunk->data);
        if (sequenceNumber != m_sequenceNumber++) {
            fallbackNotAnimated();
            return;
        }

        if (setjmp(JMPBUF(m_png))) {
            fallbackNotAnimated();
            return;
        }

        png_save_uint_32(chunk->data, chunk->size - 4);
        png_process_data(m_png, m_info, chunk->data, 4);
        memcpy(chunk->data, "IDAT", 4);
        png_process_data(m_png, m_info, chunk->data, chunk->size);
        png_process_data(m_png, m_info, chunk->data, 4);
    }
}

void PNGImageDecoder::frameHeader()
{
    int colorType = png_get_color_type(m_png, m_info);

    if (colorType == PNG_COLOR_TYPE_PALETTE)
        png_set_expand(m_png);

    int bitDepth = png_get_bit_depth(m_png, m_info);
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8)
        png_set_expand(m_png);

    if (png_get_valid(m_png, m_info, PNG_INFO_tRNS))
        png_set_expand(m_png);

    if (bitDepth == 16)
        png_set_strip_16(m_png);

    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(m_png);

    double gamma;
    if (png_get_gAMA(m_png, m_info, &gamma))
        png_set_gamma(m_png, cDefaultGamma, gamma);

    png_set_interlace_handling(m_png);

    png_read_update_info(m_png, m_info);
}

void PNGImageDecoder::init()
{
    m_isAnimated = false;
    m_frameInfo = false;
    m_frameIsHidden = false;
    m_hasInfo = false;
    m_currentFrame = 0;
    m_totalFrames = 0;
    m_sequenceNumber = 0;
}

void PNGImageDecoder::clearFrameBufferCache(size_t clearBeforeFrame)
{
    if (m_frameBufferCache.isEmpty())
        return;

    // See GIFImageDecoder for full explanation.
    clearBeforeFrame = std::min(clearBeforeFrame, m_frameBufferCache.size() - 1);
    const Vector<ScalableImageDecoderFrame>::iterator end(m_frameBufferCache.begin() + clearBeforeFrame);

    Vector<ScalableImageDecoderFrame>::iterator i(end);
    for (; (i != m_frameBufferCache.begin()) && (i->isInvalid() || (i->disposalMethod() == ScalableImageDecoderFrame::DisposalMethod::RestoreToPrevious)); --i) {
        if (i->isComplete() && (i != end))
            i->clear();
    }

    // Now |i| holds the last frame we need to preserve; clear prior frames.
    for (Vector<ScalableImageDecoderFrame>::iterator j(m_frameBufferCache.begin()); j != i; ++j) {
        ASSERT(!j->isPartial());
        if (j->isInvalid())
            j->clear();
    }
}

void PNGImageDecoder::initFrameBuffer(size_t frameIndex)
{
    if (frameIndex >= frameCount())
        return;

    auto& buffer = m_frameBufferCache[frameIndex];

    // The starting state for this frame depends on the previous frame's
    // disposal method.
    //
    // Frames that use the DisposalMethod::RestoreToPrevious method are effectively
    // no-ops in terms of changing the starting state of a frame compared to
    // the starting state of the previous frame, so skip over them.  (If the
    // first frame specifies this method, it will get treated like
    // DisposeOverwriteBgcolor below and reset to a completely empty image.)
    const auto* prevBuffer = &m_frameBufferCache[--frameIndex];
    auto prevMethod = prevBuffer->disposalMethod();
    while (frameIndex && (prevMethod == ScalableImageDecoderFrame::DisposalMethod::RestoreToPrevious)) {
        prevBuffer = &m_frameBufferCache[--frameIndex];
        prevMethod = prevBuffer->disposalMethod();
    }

    png_structp png = m_reader->pngPtr();
    ASSERT(prevBuffer->isComplete());

    if (prevMethod == ScalableImageDecoderFrame::DisposalMethod::DoNotDispose) {
        // Preserve the last frame as the starting state for this frame.
        if (!prevBuffer->backingStore() || !buffer.initialize(*prevBuffer->backingStore()))
            longjmp(JMPBUF(png), 1);
    } else {
        // We want to clear the previous frame to transparent, without
        // affecting pixels in the image outside of the frame.
        IntRect prevRect = prevBuffer->backingStore()->frameRect();
        if (!frameIndex || prevRect.contains(IntRect(IntPoint(), scaledSize()))) {
            // Clearing the first frame, or a frame the size of the whole
            // image, results in a completely empty image.
            buffer.backingStore()->clear();
            buffer.setHasAlpha(true);
        } else {
            // Copy the whole previous buffer, then clear just its frame.
            if (!prevBuffer->backingStore() || !buffer.initialize(*prevBuffer->backingStore())) {
                longjmp(JMPBUF(png), 1);
                return;
            }
            buffer.backingStore()->clearRect(prevRect);
            buffer.setHasAlpha(true);
        }
    }
    
    IntRect frameRect(m_xOffset, m_yOffset, m_width, m_height);

    // Make sure the frameRect doesn't extend outside the buffer.
    if (frameRect.maxX() > size().width())
        frameRect.setWidth(size().width() - m_xOffset);
    if (frameRect.maxY() > size().height())
        frameRect.setHeight(size().height() - m_yOffset);

    int left = upperBoundScaledX(frameRect.x());
    int right = lowerBoundScaledX(frameRect.maxX(), left);
    int top = upperBoundScaledY(frameRect.y());
    int bottom = lowerBoundScaledY(frameRect.maxY(), top);
    buffer.backingStore()->setFrameRect(IntRect(left, top, right - left, bottom - top));
}

void PNGImageDecoder::frameComplete()
{
    if (m_frameIsHidden || m_currentFrame >= frameCount())
        return;

    auto& buffer = m_frameBufferCache[m_currentFrame];
    buffer.setDecodingStatus(DecodingStatus::Complete);

    png_bytep interlaceBuffer = m_reader->interlaceBuffer();

    if (m_currentFrame && interlaceBuffer) {
        IntRect rect = buffer.backingStore()->frameRect();
        bool hasAlpha = m_reader->hasAlpha();
        unsigned colorChannels = hasAlpha ? 4 : 3;
        bool nonTrivialAlpha = false;
        if (m_blend && !hasAlpha)
            m_blend = 0;

        ASSERT(!m_scaled);
        png_bytep row = interlaceBuffer;
        for (int y = rect.y(); y < rect.maxY(); ++y, row += colorChannels * size().width()) {
            png_bytep pixel = row;
            auto* address = buffer.backingStore()->pixelAt(rect.x(), y);
            for (int x = rect.x(); x < rect.maxX(); ++x, pixel += colorChannels) {
                unsigned alpha = hasAlpha ? pixel[3] : 255;
                nonTrivialAlpha |= alpha < 255;
                if (!m_blend)
                    buffer.backingStore()->setPixel(address++, pixel[0], pixel[1], pixel[2], alpha);
                else
                    buffer.backingStore()->blendPixel(address++, pixel[0], pixel[1], pixel[2], alpha);
            }
        }

        if (!nonTrivialAlpha) {
            IntRect rect = buffer.backingStore()->frameRect();
            if (rect.contains(IntRect(IntPoint(), scaledSize())))
                buffer.setHasAlpha(false);
            else {
                size_t frameIndex = m_currentFrame;
                const auto* prevBuffer = &m_frameBufferCache[--frameIndex];
                while (frameIndex && (prevBuffer->disposalMethod() == ScalableImageDecoderFrame::DisposalMethod::RestoreToPrevious))
                    prevBuffer = &m_frameBufferCache[--frameIndex];

                IntRect prevRect = prevBuffer->backingStore()->frameRect();
                if ((prevBuffer->disposalMethod() == ScalableImageDecoderFrame::DisposalMethod::RestoreToBackground) && !prevBuffer->hasAlpha() && rect.contains(prevRect))
                    buffer.setHasAlpha(false);
            }
        } else if (!m_blend && !buffer.hasAlpha())
            buffer.setHasAlpha(nonTrivialAlpha);
    }
    m_currentFrame++;
}

int PNGImageDecoder::processingStart(png_unknown_chunkp chunk)
{
    static png_byte dataPNG[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    static png_byte datagAMA[16] = {0, 0, 0, 4, 103, 65, 77, 65};

    if (!m_hasInfo)
        return 0;

    m_totalFrames++;

    m_png = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, decodingFailed, 0);
    m_info = png_create_info_struct(m_png);
    if (setjmp(JMPBUF(m_png)))
        return 1;

    png_set_crc_action(m_png, PNG_CRC_QUIET_USE, PNG_CRC_QUIET_USE);
    png_set_progressive_read_fn(m_png, static_cast<png_voidp>(this),
        WebCore::frameHeader, WebCore::rowAvailable, 0);

    memcpy(m_dataIHDR + 8, chunk->data + 4, 8);
    png_save_uint_32(datagAMA + 8, m_gamma);

    png_process_data(m_png, m_info, dataPNG, 8);
    png_process_data(m_png, m_info, m_dataIHDR, 25);
    png_process_data(m_png, m_info, datagAMA, 16);
    if (m_sizePLTE > 0)
        png_process_data(m_png, m_info, m_dataPLTE, m_sizePLTE);
    if (m_sizetRNS > 0)
        png_process_data(m_png, m_info, m_datatRNS, m_sizetRNS);

    return 0;
}

int PNGImageDecoder::processingFinish()
{
    static png_byte dataIEND[12] = {0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130};

    if (!m_hasInfo)
        return 0;

    if (m_totalFrames) {
        if (setjmp(JMPBUF(m_png)))
            return 1;

        png_process_data(m_png, m_info, dataIEND, 12);
        png_destroy_read_struct(&m_png, &m_info, 0);
    }

    frameComplete();
    return 0;
}

void PNGImageDecoder::fallbackNotAnimated()
{
    m_isAnimated = false;
    m_playCount = 0;
    m_currentFrame = 0;
}
#endif

} // namespace WebCore
