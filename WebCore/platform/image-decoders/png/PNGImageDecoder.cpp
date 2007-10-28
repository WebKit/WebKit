/*
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * Portions are Copyright (C) 2001 mozilla.org
 *
 * Other contributors:
 *   Stuart Parmenter <stuart@mozilla.com>
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

#include "PNGImageDecoder.h"
#include "png.h"
#include "assert.h"

#if PLATFORM(CAIRO) || PLATFORM(QT) || PLATFORM(WX)

namespace WebCore {

// Gamma constants.
const double cMaxGamma = 21474.83;
const double cDefaultGamma = 2.2;
const double cInverseGamma = 0.45455;

// Protect against large PNGs. See Mozilla's bug #251381 for more info.
const long cMaxPNGSize = 1000000L;

// Called if the decoding of the image fails.
static void PNGAPI decodingFailed(png_structp png_ptr, png_const_charp error_msg);

// Callbacks given to the read struct.  The first is for warnings (we want to treat a particular warning
// as an error, which is why we have to register this callback.
static void PNGAPI decodingWarning(png_structp png_ptr, png_const_charp warning_msg);

// Called when we have obtained the header information (including the size).
static void PNGAPI headerAvailable(png_structp png_ptr, png_infop info_ptr);

// Called when a row is ready.
static void PNGAPI rowAvailable(png_structp png_ptr, png_bytep new_row,
                                png_uint_32 row_num, int pass);

// Called when we have completely finished decoding the image.
static void PNGAPI pngComplete(png_structp png_ptr, png_infop info_ptr);

class PNGImageReader
{
public:
    PNGImageReader(PNGImageDecoder* decoder)
    : m_readOffset(0), m_decodingSizeOnly(false), m_interlaceBuffer(0), m_hasAlpha(0)
    {
        m_png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, decodingFailed, decodingWarning);
        m_info = png_create_info_struct(m_png);
        png_set_progressive_read_fn(m_png, decoder, headerAvailable, rowAvailable, pngComplete);
    }

    ~PNGImageReader()
    {
        close();
    }

    void close() {
        if (m_png && m_info)
            png_destroy_read_struct(&m_png, &m_info, 0);
        delete []m_interlaceBuffer;
        m_readOffset = 0;
    }

    void decode(const Vector<char>& data, bool sizeOnly)
    {
        m_decodingSizeOnly = sizeOnly;

        // We need to do the setjmp here. Otherwise bad things will happen
        if (setjmp(m_png->jmpbuf)) {
            close();
            return;
        }

        // Go ahead and assume we consumed all the data.  If we consume less, the
        // callback will adjust our read offset accordingly.  Do not attempt to adjust the
        // offset after png_process_data returns.
        unsigned offset = m_readOffset;
        unsigned remaining = data.size() - m_readOffset;
        m_readOffset = data.size();
        png_process_data(m_png, m_info, (png_bytep)(data.data()) + offset, remaining);
    }

    bool decodingSizeOnly() const { return m_decodingSizeOnly; }
    png_structp pngPtr() const { return m_png; }
    png_infop infoPtr() const { return m_info; }
    png_bytep interlaceBuffer() const { return m_interlaceBuffer; }
    bool hasAlpha() const { return m_hasAlpha; }

    void setReadOffset(unsigned offset) { m_readOffset = offset; }
    void setHasAlpha(bool b) { m_hasAlpha = b; }

    void createInterlaceBuffer(int size) {
        m_interlaceBuffer = new png_byte[size];
    }

private:
    unsigned m_readOffset;
    bool m_decodingSizeOnly;
    png_structp m_png;
    png_infop m_info;
    png_bytep m_interlaceBuffer;
    bool m_hasAlpha;
};

PNGImageDecoder::PNGImageDecoder()
: m_reader(0)
{
    m_frameBufferCache.resize(1);
}

PNGImageDecoder::~PNGImageDecoder()
{
    delete m_reader;
}

// Take the data and store it.
void PNGImageDecoder::setData(SharedBuffer* data, bool allDataReceived)
{
    if (m_failed)
        return;

    // Cache our new data.
    ImageDecoder::setData(data, allDataReceived);

    // Create the PNG reader.
    if (!m_reader && !m_failed)
        m_reader = new PNGImageReader(this);
}

// Whether or not the size information has been decoded yet.
bool PNGImageDecoder::isSizeAvailable() const
{
    // If we have pending data to decode, send it to the PNG reader now.
    if (!m_sizeAvailable && m_reader) {
        if (m_failed)
            return false;

        // The decoder will go ahead and aggressively consume everything up until the
        // size is encountered.
        decode(true);
    }

    return m_sizeAvailable;
}

RGBA32Buffer* PNGImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index)
        return 0;

    RGBA32Buffer& frame = m_frameBufferCache[0];
    if (frame.status() != RGBA32Buffer::FrameComplete && m_reader)
        // Decode this frame.
        decode();
    return &frame;
}

// Feed data to the PNG reader.
void PNGImageDecoder::decode(bool sizeOnly) const
{
    if (m_failed)
        return;

    m_reader->decode(m_data->buffer(), sizeOnly);
    
    if (m_failed || (m_frameBufferCache[0].status() == RGBA32Buffer::FrameComplete)) {
        delete m_reader;
        m_reader = 0;
    }
}

void decodingFailed(png_structp png, png_const_charp errorMsg)
{
    static_cast<PNGImageDecoder*>(png_get_progressive_ptr(png))->decodingFailed();
    longjmp(png->jmpbuf, 1);
}

void decodingWarning(png_structp png, png_const_charp warningMsg)
{
  // Mozilla did this, so we will too.
  // Convert a tRNS warning to be an error (documented in bugzilla.mozilla.org bug #251381)
  if (!strncmp(warningMsg, "Missing PLTE before tRNS", 24))
      png_error(png, warningMsg);
}

void headerAvailable(png_structp png, png_infop info)
{
    static_cast<PNGImageDecoder*>(png_get_progressive_ptr(png))->headerAvailable();
}

void PNGImageDecoder::headerAvailable()
{
    png_structp png = reader()->pngPtr();
    png_infop info = reader()->infoPtr();
    png_uint_32 width = png->width;
    png_uint_32 height = png->height;
    
    // Protect against large images.
    if (png->width > cMaxPNGSize || png->height > cMaxPNGSize) {
        m_failed = true;
        longjmp(png->jmpbuf, 1);
        return;
    }
    
    // We can fill in the size now that the header is available.
    if (!m_sizeAvailable) {
        m_sizeAvailable = true;
        m_size = IntSize(width, height);
    }

    int bitDepth, colorType, interlaceType, compressionType, filterType, channels;
    png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType,
                 &interlaceType, &compressionType, &filterType);

    // The options we set here match what Mozilla does.

    // Expand to ensure we use 24-bit for RGB and 32-bit for RGBA.
    if (colorType == PNG_COLOR_TYPE_PALETTE ||
        (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8))
        png_set_expand(png);
    
    png_bytep trns = 0;
    int trnsCount = 0;
    if (png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_get_tRNS(png, info, &trns, &trnsCount, 0);
        png_set_expand(png);
    }

    if (bitDepth == 16)
        png_set_strip_16(png);

    if (colorType == PNG_COLOR_TYPE_GRAY ||
        colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);

    // Deal with gamma and keep it under our control.
    double gamma;
    if (png_get_gAMA(png, info, &gamma)) {
        if ((gamma <= 0.0) || (gamma > cMaxGamma)) {
            gamma = cInverseGamma;
            png_set_gAMA(png, info, gamma);
        }
        png_set_gamma(png, cDefaultGamma, gamma);
    }
    else
        png_set_gamma(png, cDefaultGamma, cInverseGamma);

    // Tell libpng to send us rows for interlaced pngs.
    if (interlaceType == PNG_INTERLACE_ADAM7)
        png_set_interlace_handling(png);

    // Update our info now
    png_read_update_info(png, info);
    channels = png_get_channels(png, info);
    assert(channels == 3 || channels == 4);

    reader()->setHasAlpha(channels == 4);

    if (reader()->decodingSizeOnly()) {
        // If we only needed the size, halt the reader.     
        reader()->setReadOffset(m_data->size() - png->buffer_size);
        png->buffer_size = 0;
    }
}

void rowAvailable(png_structp png, png_bytep rowBuffer,
                  png_uint_32 rowIndex, int interlacePass)
{
    static_cast<PNGImageDecoder*>(png_get_progressive_ptr(png))->rowAvailable(rowBuffer, rowIndex, interlacePass);
}

void PNGImageDecoder::rowAvailable(unsigned char* rowBuffer, unsigned rowIndex, int interlacePass)
{
    // Resize to the width and height of the image.
    RGBA32Buffer& buffer = m_frameBufferCache[0];
    if (buffer.status() == RGBA32Buffer::FrameEmpty) {
        // Let's resize our buffer now to the correct width/height.
        RGBA32Array& bytes = buffer.bytes();
        bytes.resize(m_size.width() * m_size.height());

        // Update our status to be partially complete.
        buffer.setStatus(RGBA32Buffer::FramePartial);

        // For PNGs, the frame always fills the entire image.
        buffer.setRect(IntRect(0, 0, m_size.width(), m_size.height()));

        if (reader()->pngPtr()->interlaced)
            reader()->createInterlaceBuffer((reader()->hasAlpha() ? 4 : 3) * m_size.width() * m_size.height());
    }

    if (rowBuffer == 0)
        return;

   /* libpng comments (pasted in here to explain what follows)
    *
    * this function is called for every row in the image.  If the
    * image is interlacing, and you turned on the interlace handler,
    * this function will be called for every row in every pass.
    * Some of these rows will not be changed from the previous pass.
    * When the row is not changed, the new_row variable will be NULL.
    * The rows and passes are called in order, so you don't really
    * need the row_num and pass, but I'm supplying them because it
    * may make your life easier.
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
    
    png_structp png = reader()->pngPtr();
    bool hasAlpha = reader()->hasAlpha();
    unsigned colorChannels = hasAlpha ? 4 : 3;
    png_bytep row;
    png_bytep interlaceBuffer = reader()->interlaceBuffer();
    if (interlaceBuffer) {
        row = interlaceBuffer + (rowIndex * colorChannels * m_size.width());
        png_progressive_combine_row(png, row, rowBuffer);
    }
    else
        row = rowBuffer;

    // Copy the data into our buffer.
    int width = m_size.width();
    unsigned* dst = buffer.bytes().data() + rowIndex * width;
    bool sawAlpha = false;
    for (int i = 0; i < width; i++) {
        unsigned red = *row++;
        unsigned green = *row++;
        unsigned blue = *row++;
        unsigned alpha = (hasAlpha ? *row++ : 255);
        RGBA32Buffer::setRGBA(*dst++, red, green, blue, alpha);
        if (!sawAlpha && alpha < 255) {
            sawAlpha = true;
            buffer.setHasAlpha(true);
        }
    }

    buffer.ensureHeight(rowIndex + 1);
}

void pngComplete(png_structp png, png_infop info)
{
    static_cast<PNGImageDecoder*>(png_get_progressive_ptr(png))->pngComplete();
}

void PNGImageDecoder::pngComplete()
{
    // Hand back an appropriately sized buffer, even if the image ended up being empty.
    RGBA32Buffer& buffer = m_frameBufferCache[0];
    buffer.setStatus(RGBA32Buffer::FrameComplete);
}

}

#endif // PLATFORM(CAIRO)
