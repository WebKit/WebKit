/*
 * Copyright (C) 2006 Apple Inc.
 *
 * Portions are Copyright (C) 2001-6 mozilla.org
 *
 * Other contributors:
 *   Stuart Parmenter <stuart@mozilla.com>
 *
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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
#include "JPEGImageDecoder.h"

extern "C" {
#if USE(ICCJPEG)
#include <iccjpeg.h>
#endif
#include <setjmp.h>
}

#if CPU(BIG_ENDIAN) || CPU(MIDDLE_ENDIAN)
#define ASSUME_LITTLE_ENDIAN 0
#else
#define ASSUME_LITTLE_ENDIAN 1
#endif

#if defined(JCS_ALPHA_EXTENSIONS) && ASSUME_LITTLE_ENDIAN
#define TURBO_JPEG_RGB_SWIZZLE
inline J_COLOR_SPACE rgbOutputColorSpace() { return JCS_EXT_BGRA; }
inline bool turboSwizzled(J_COLOR_SPACE colorSpace) { return colorSpace == JCS_EXT_RGBA || colorSpace == JCS_EXT_BGRA; }
#else
inline J_COLOR_SPACE rgbOutputColorSpace() { return JCS_RGB; }
#endif

#if USE(LOW_QUALITY_IMAGE_NO_JPEG_DITHERING)
inline J_DCT_METHOD dctMethod() { return JDCT_IFAST; }
inline J_DITHER_MODE ditherMode() { return JDITHER_NONE; }
#else
inline J_DCT_METHOD dctMethod() { return JDCT_ISLOW; }
inline J_DITHER_MODE ditherMode() { return JDITHER_FS; }
#endif

#if USE(LOW_QUALITY_IMAGE_NO_JPEG_FANCY_UPSAMPLING)
inline bool doFancyUpsampling() { return false; }
#else
inline bool doFancyUpsampling() { return true; }
#endif

const int exifMarker = JPEG_APP0 + 1;

namespace WebCore {

struct decoder_error_mgr {
    struct jpeg_error_mgr pub; // "public" fields for IJG library
    jmp_buf setjmp_buffer;     // For handling catastropic errors
};

enum jstate {
    JPEG_HEADER,                 // Reading JFIF headers
    JPEG_START_DECOMPRESS,
    JPEG_DECOMPRESS_PROGRESSIVE, // Output progressive pixels
    JPEG_DECOMPRESS_SEQUENTIAL,  // Output sequential pixels
    JPEG_DONE,
    JPEG_ERROR
};

void init_source(j_decompress_ptr jd);
boolean fill_input_buffer(j_decompress_ptr jd);
void skip_input_data(j_decompress_ptr jd, long num_bytes);
void term_source(j_decompress_ptr jd);
void error_exit(j_common_ptr cinfo);

// Implementation of a JPEG src object that understands our state machine
struct decoder_source_mgr {
    // public fields; must be first in this struct!
    struct jpeg_source_mgr pub;

    JPEGImageReader* decoder;
};

static unsigned readUint16(JOCTET* data, bool isBigEndian)
{
    if (isBigEndian)
        return (GETJOCTET(data[0]) << 8) | GETJOCTET(data[1]);
    return (GETJOCTET(data[1]) << 8) | GETJOCTET(data[0]);
}

static unsigned readUint32(JOCTET* data, bool isBigEndian)
{
    if (isBigEndian)
        return (GETJOCTET(data[0]) << 24) | (GETJOCTET(data[1]) << 16) | (GETJOCTET(data[2]) << 8) | GETJOCTET(data[3]);
    return (GETJOCTET(data[3]) << 24) | (GETJOCTET(data[2]) << 16) | (GETJOCTET(data[1]) << 8) | GETJOCTET(data[0]);
}

static bool checkExifHeader(jpeg_saved_marker_ptr marker, bool& isBigEndian, unsigned& ifdOffset)
{
    // For exif data, the APP1 block is followed by 'E', 'x', 'i', 'f', '\0',
    // then a fill byte, and then a tiff file that contains the metadata.
    // A tiff file starts with 'I', 'I' (intel / little endian byte order) or
    // 'M', 'M' (motorola / big endian byte order), followed by (uint16_t)42,
    // followed by an uint32_t with the offset to the tag block, relative to the
    // tiff file start.
    const unsigned exifHeaderSize = 14;
    if (!(marker->marker == exifMarker
        && marker->data_length >= exifHeaderSize
        && marker->data[0] == 'E'
        && marker->data[1] == 'x'
        && marker->data[2] == 'i'
        && marker->data[3] == 'f'
        && marker->data[4] == '\0'
        // data[5] is a fill byte
        && ((marker->data[6] == 'I' && marker->data[7] == 'I')
            || (marker->data[6] == 'M' && marker->data[7] == 'M'))))
        return false;

    isBigEndian = marker->data[6] == 'M';
    if (readUint16(marker->data + 8, isBigEndian) != 42)
        return false;

    ifdOffset = readUint32(marker->data + 10, isBigEndian);
    return true;
}

static ImageOrientation readImageOrientation(jpeg_decompress_struct* info)
{
    // The JPEG decoder looks at EXIF metadata.
    // FIXME: Possibly implement XMP and IPTC support.
    const unsigned orientationTag = 0x112;
    const unsigned shortType = 3;
    for (jpeg_saved_marker_ptr marker = info->marker_list; marker; marker = marker->next) {
        bool isBigEndian;
        unsigned ifdOffset;
        if (!checkExifHeader(marker, isBigEndian, ifdOffset))
            continue;
        const unsigned offsetToTiffData = 6; // Account for 'Exif\0<fill byte>' header.
        if (marker->data_length < offsetToTiffData || ifdOffset >= marker->data_length - offsetToTiffData)
            continue;
        ifdOffset += offsetToTiffData;

        // The jpeg exif container format contains a tiff block for metadata.
        // A tiff image file directory (ifd) consists of a uint16_t describing
        // the number of ifd entries, followed by that many entries.
        // When touching this code, it's useful to look at the tiff spec:
        // http://partners.adobe.com/public/developer/en/tiff/TIFF6.pdf
        JOCTET* ifd = marker->data + ifdOffset;
        JOCTET* end = marker->data + marker->data_length;
        if (end - ifd < 2)
            continue;
        unsigned tagCount = readUint16(ifd, isBigEndian);
        ifd += 2; // Skip over the uint16 that was just read.

        // Every ifd entry is 2 bytes of tag, 2 bytes of contents datatype,
        // 4 bytes of number-of-elements, and 4 bytes of either offset to the
        // tag data, or if the data is small enough, the inlined data itself.
        const int ifdEntrySize = 12;
        for (unsigned i = 0; i < tagCount && end - ifd >= ifdEntrySize; ++i, ifd += ifdEntrySize) {
            unsigned tag = readUint16(ifd, isBigEndian);
            unsigned type = readUint16(ifd + 2, isBigEndian);
            unsigned count = readUint32(ifd + 4, isBigEndian);
            if (tag == orientationTag && type == shortType && count == 1)
                return ImageOrientation::fromEXIFValue(readUint16(ifd + 8, isBigEndian));
        }
    }

    return ImageOrientation::None;
}

class JPEGImageReader {
    WTF_MAKE_FAST_ALLOCATED;
public:
    JPEGImageReader(JPEGImageDecoder* decoder)
        : m_decoder(decoder)
        , m_bufferLength(0)
        , m_bytesToSkip(0)
        , m_state(JPEG_HEADER)
        , m_samples(0)
    {
        memset(&m_info, 0, sizeof(jpeg_decompress_struct));

        // We set up the normal JPEG error routines, then override error_exit.
        m_info.err = jpeg_std_error(&m_err.pub);
        m_err.pub.error_exit = error_exit;

        // Allocate and initialize JPEG decompression object.
        jpeg_create_decompress(&m_info);

        decoder_source_mgr* src = 0;
        if (!m_info.src) {
            src = (decoder_source_mgr*)fastCalloc(sizeof(decoder_source_mgr), 1);
            if (!src) {
                m_state = JPEG_ERROR;
                return;
            }
        }

        m_info.src = (jpeg_source_mgr*)src;

        // Set up callback functions.
        src->pub.init_source = init_source;
        src->pub.fill_input_buffer = fill_input_buffer;
        src->pub.skip_input_data = skip_input_data;
        src->pub.resync_to_restart = jpeg_resync_to_restart;
        src->pub.term_source = term_source;
        src->decoder = this;

#if USE(ICCJPEG)
        // Retain ICC color profile markers for color management.
        setup_read_icc_profile(&m_info);
#endif

        // Keep APP1 blocks, for obtaining exif data.
        jpeg_save_markers(&m_info, exifMarker, 0xFFFF);
    }

    ~JPEGImageReader()
    {
        close();
    }

    void close()
    {
        decoder_source_mgr* src = (decoder_source_mgr*)m_info.src;
        if (src)
            fastFree(src);
        m_info.src = 0;

        jpeg_destroy_decompress(&m_info);
    }

    void skipBytes(long numBytes)
    {
        decoder_source_mgr* src = (decoder_source_mgr*)m_info.src;
        long bytesToSkip = std::min(numBytes, (long)src->pub.bytes_in_buffer);
        src->pub.bytes_in_buffer -= (size_t)bytesToSkip;
        src->pub.next_input_byte += bytesToSkip;

        m_bytesToSkip = std::max(numBytes - bytesToSkip, static_cast<long>(0));
    }

    bool decode(const SharedBuffer::DataSegment& data, bool onlySize)
    {
        m_decodingSizeOnly = onlySize;

        unsigned newByteCount = data.size() - m_bufferLength;
        unsigned readOffset = m_bufferLength - m_info.src->bytes_in_buffer;

        m_info.src->bytes_in_buffer += newByteCount;
        m_info.src->next_input_byte = (JOCTET*)(data.data()) + readOffset;

        // If we still have bytes to skip, try to skip those now.
        if (m_bytesToSkip)
            skipBytes(m_bytesToSkip);

        m_bufferLength = data.size();

        // We need to do the setjmp here. Otherwise bad things will happen
        if (setjmp(m_err.setjmp_buffer))
            return m_decoder->setFailed();

        switch (m_state) {
        case JPEG_HEADER:
            // Read file parameters with jpeg_read_header().
            if (jpeg_read_header(&m_info, TRUE) == JPEG_SUSPENDED)
                return false; // I/O suspension.

            switch (m_info.jpeg_color_space) {
            case JCS_GRAYSCALE:
            case JCS_RGB:
            case JCS_YCbCr:
                // libjpeg can convert GRAYSCALE and YCbCr image pixels to RGB.
                m_info.out_color_space = rgbOutputColorSpace();
                break;
            case JCS_CMYK:
            case JCS_YCCK:
                // libjpeg can convert YCCK to CMYK, but neither to RGB, so we
                // manually convert CMKY to RGB.
                m_info.out_color_space = JCS_CMYK;
                break;
            default:
                return m_decoder->setFailed();
            }

            m_state = JPEG_START_DECOMPRESS;

            // We can fill in the size now that the header is available.
            if (!m_decoder->setSize(IntSize(m_info.image_width, m_info.image_height)))
                return false;

            m_decoder->setOrientation(readImageOrientation(info()));

            // Don't allocate a giant and superfluous memory buffer when the
            // image is a sequential JPEG.
            m_info.buffered_image = jpeg_has_multiple_scans(&m_info);

            // Used to set up image size so arrays can be allocated.
            jpeg_calc_output_dimensions(&m_info);

            // Make a one-row-high sample array that will go away when done with
            // image. Always make it big enough to hold an RGB row. Since this
            // uses the IJG memory manager, it must be allocated before the call
            // to jpeg_start_compress().
            // FIXME: note that some output color spaces do not need the samples
            // buffer. Remove this allocation for those color spaces.
            m_samples = (*m_info.mem->alloc_sarray)((j_common_ptr) &m_info, JPOOL_IMAGE, m_info.output_width * 4, 1);

            if (m_decodingSizeOnly) {
                // We can stop here. Reduce our buffer length and available data.
                m_bufferLength -= m_info.src->bytes_in_buffer;
                m_info.src->bytes_in_buffer = 0;
                return true;
            }
            FALLTHROUGH;

        case JPEG_START_DECOMPRESS:
            // Set parameters for decompression.
            // FIXME -- Should reset dct_method and dither mode for final pass
            // of progressive JPEG.
            m_info.dct_method = dctMethod();
            m_info.dither_mode = ditherMode();
            m_info.do_fancy_upsampling = doFancyUpsampling() ? TRUE : FALSE;
            m_info.enable_2pass_quant = FALSE;
            m_info.do_block_smoothing = TRUE;

            // Start decompressor.
            if (!jpeg_start_decompress(&m_info))
                return false; // I/O suspension.

            // If this is a progressive JPEG ...
            m_state = (m_info.buffered_image) ? JPEG_DECOMPRESS_PROGRESSIVE : JPEG_DECOMPRESS_SEQUENTIAL;
            FALLTHROUGH;

        case JPEG_DECOMPRESS_SEQUENTIAL:
            if (m_state == JPEG_DECOMPRESS_SEQUENTIAL) {

                if (!m_decoder->outputScanlines())
                    return false; // I/O suspension.

                // If we've completed image output...
                ASSERT(m_info.output_scanline == m_info.output_height);
                m_state = JPEG_DONE;
            }
            FALLTHROUGH;

        case JPEG_DECOMPRESS_PROGRESSIVE:
            if (m_state == JPEG_DECOMPRESS_PROGRESSIVE) {
                int status;
                do {
                    status = jpeg_consume_input(&m_info);
                } while ((status != JPEG_SUSPENDED) && (status != JPEG_REACHED_EOI));

                for (;;) {
                    if (!m_info.output_scanline) {
                        int scan = m_info.input_scan_number;

                        // If we haven't displayed anything yet
                        // (output_scan_number == 0) and we have enough data for
                        // a complete scan, force output of the last full scan.
                        if (!m_info.output_scan_number && (scan > 1) && (status != JPEG_REACHED_EOI))
                            --scan;

                        if (!jpeg_start_output(&m_info, scan))
                            return false; // I/O suspension.
                    }

                    if (m_info.output_scanline == 0xffffff)
                        m_info.output_scanline = 0;

                    // If outputScanlines() fails, it deletes |this|. Therefore,
                    // copy the decoder pointer and use it to check for failure
                    // to avoid member access in the failure case.
                    JPEGImageDecoder* decoder = m_decoder;
                    if (!decoder->outputScanlines()) {
                        if (decoder->failed()) // Careful; |this| is deleted.
                            return false;
                        if (!m_info.output_scanline)
                            // Didn't manage to read any lines - flag so we
                            // don't call jpeg_start_output() multiple times for
                            // the same scan.
                            m_info.output_scanline = 0xffffff;
                        return false; // I/O suspension.
                    }

                    if (m_info.output_scanline == m_info.output_height) {
                        if (!jpeg_finish_output(&m_info))
                            return false; // I/O suspension.

                        if (jpeg_input_complete(&m_info) && (m_info.input_scan_number == m_info.output_scan_number))
                            break;

                        m_info.output_scanline = 0;
                    }
                }

                m_state = JPEG_DONE;
            }
            FALLTHROUGH;

        case JPEG_DONE:
            // Finish decompression.
            return jpeg_finish_decompress(&m_info);

        case JPEG_ERROR:
            // We can get here if the constructor failed.
            return m_decoder->setFailed();
        }

        return true;
    }

    jpeg_decompress_struct* info() { return &m_info; }
    JSAMPARRAY samples() const { return m_samples; }
    JPEGImageDecoder* decoder() { return m_decoder; }

private:
    JPEGImageDecoder* m_decoder;
    unsigned m_bufferLength;
    int m_bytesToSkip;
    bool m_decodingSizeOnly;

    jpeg_decompress_struct m_info;
    decoder_error_mgr m_err;
    jstate m_state;

    JSAMPARRAY m_samples;
};

// Override the standard error method in the IJG JPEG decoder code.
void error_exit(j_common_ptr cinfo)
{
    // Return control to the setjmp point.
    decoder_error_mgr *err = reinterpret_cast_ptr<decoder_error_mgr *>(cinfo->err);
    longjmp(err->setjmp_buffer, -1);
}

void init_source(j_decompress_ptr)
{
}

void skip_input_data(j_decompress_ptr jd, long num_bytes)
{
    decoder_source_mgr *src = (decoder_source_mgr *)jd->src;
    src->decoder->skipBytes(num_bytes);
}

boolean fill_input_buffer(j_decompress_ptr)
{
    // Our decode step always sets things up properly, so if this method is ever
    // called, then we have hit the end of the buffer.  A return value of false
    // indicates that we have no data to supply yet.
    return FALSE;
}

void term_source(j_decompress_ptr jd)
{
    decoder_source_mgr *src = (decoder_source_mgr *)jd->src;
    src->decoder->decoder()->jpegComplete();
}

JPEGImageDecoder::JPEGImageDecoder(AlphaOption alphaOption, GammaAndColorProfileOption gammaAndColorProfileOption)
    : ScalableImageDecoder(alphaOption, gammaAndColorProfileOption)
{
}

JPEGImageDecoder::~JPEGImageDecoder() = default;

ScalableImageDecoderFrame* JPEGImageDecoder::frameBufferAtIndex(size_t index)
{
    if (index)
        return 0;

    if (m_frameBufferCache.isEmpty())
        m_frameBufferCache.grow(1);

    auto& frame = m_frameBufferCache[0];
    if (!frame.isComplete())
        decode(false, isAllDataReceived());
    return &frame;
}

bool JPEGImageDecoder::setFailed()
{
    m_reader = nullptr;
    return ScalableImageDecoder::setFailed();
}

template <J_COLOR_SPACE colorSpace>
void setPixel(ScalableImageDecoderFrame& buffer, uint32_t* currentAddress, JSAMPARRAY samples, int column)
{
    JSAMPLE* jsample = *samples + column * (colorSpace == JCS_RGB ? 3 : 4);

    switch (colorSpace) {
    case JCS_RGB:
        buffer.backingStore()->setPixel(currentAddress, jsample[0], jsample[1], jsample[2], 0xFF);
        break;
    case JCS_CMYK:
        // Source is 'Inverted CMYK', output is RGB.
        // See: http://www.easyrgb.com/math.php?MATH=M12#text12
        // Or: http://www.ilkeratalay.com/colorspacesfaq.php#rgb
        // From CMYK to CMY:
        // X =   X    * (1 -   K   ) +   K  [for X = C, M, or Y]
        // Thus, from Inverted CMYK to CMY is:
        // X = (1-iX) * (1 - (1-iK)) + (1-iK) => 1 - iX*iK
        // From CMY (0..1) to RGB (0..1):
        // R = 1 - C => 1 - (1 - iC*iK) => iC*iK  [G and B similar]
        unsigned k = jsample[3];
        buffer.backingStore()->setPixel(currentAddress, jsample[0] * k / 255, jsample[1] * k / 255, jsample[2] * k / 255, 0xFF);
        break;
    }
}

template <J_COLOR_SPACE colorSpace>
bool JPEGImageDecoder::outputScanlines(ScalableImageDecoderFrame& buffer)
{
    JSAMPARRAY samples = m_reader->samples();
    jpeg_decompress_struct* info = m_reader->info();
    int width = info->output_width;

    while (info->output_scanline < info->output_height) {
        // jpeg_read_scanlines will increase the scanline counter, so we
        // save the scanline before calling it.
        int sourceY = info->output_scanline;
        /* Request one scanline.  Returns 0 or 1 scanlines. */
        if (jpeg_read_scanlines(info, samples, 1) != 1)
            return false;

        auto* currentAddress = buffer.backingStore()->pixelAt(0, sourceY);
        for (int x = 0; x < width; ++x) {
            setPixel<colorSpace>(buffer, currentAddress, samples, x);
            ++currentAddress;
        }
    }
    return true;
}

bool JPEGImageDecoder::outputScanlines()
{
    if (m_frameBufferCache.isEmpty())
        return false;

    // Initialize the framebuffer if needed.
    auto& buffer = m_frameBufferCache[0];
    if (buffer.isInvalid()) {
        if (!buffer.initialize(size(), m_premultiplyAlpha))
            return setFailed();
        buffer.setDecodingStatus(DecodingStatus::Partial);
        // The buffer is transparent outside the decoded area while the image is
        // loading. The completed image will be marked fully opaque in jpegComplete().
        buffer.setHasAlpha(true);
    }

    jpeg_decompress_struct* info = m_reader->info();

#if defined(TURBO_JPEG_RGB_SWIZZLE)
    if (turboSwizzled(info->out_color_space)) {
        while (info->output_scanline < info->output_height) {
            unsigned char* row = reinterpret_cast<unsigned char*>(buffer.backingStore()->pixelAt(0, info->output_scanline));
            if (jpeg_read_scanlines(info, &row, 1) != 1)
                return false;
         }
         return true;
     }
#endif

    switch (info->out_color_space) {
    // The code inside outputScanlines<int> will be executed
    // for each pixel, so we want to avoid any extra comparisons there.
    // That is why we use template and template specializations here so
    // the proper code will be generated at compile time.
    case JCS_RGB:
        return outputScanlines<JCS_RGB>(buffer);
    case JCS_CMYK:
        return outputScanlines<JCS_CMYK>(buffer);
    default:
        ASSERT_NOT_REACHED();
    }

    return setFailed();
}

void JPEGImageDecoder::jpegComplete()
{
    if (m_frameBufferCache.isEmpty())
        return;

    // Hand back an appropriately sized buffer, even if the image ended up being
    // empty.
    auto& buffer = m_frameBufferCache[0];
    buffer.setHasAlpha(false);
    buffer.setDecodingStatus(DecodingStatus::Complete);
}

void JPEGImageDecoder::decode(bool onlySize, bool allDataReceived)
{
    if (failed())
        return;

    if (!m_reader)
        m_reader = makeUnique<JPEGImageReader>(this);

    // If we couldn't decode the image but we've received all the data, decoding
    // has failed.
    if (!m_reader->decode(*m_data, onlySize) && allDataReceived)
        setFailed();
    // If we're done decoding the image, we don't need the JPEGImageReader
    // anymore.  (If we failed, |m_reader| has already been cleared.)
    else if (!m_frameBufferCache.isEmpty() && (m_frameBufferCache[0].isComplete()))
        m_reader = nullptr;
}

}
