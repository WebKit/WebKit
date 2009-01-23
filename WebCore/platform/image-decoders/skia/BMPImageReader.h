/*
 * Copyright (c) 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BMPImageReader_h
#define BMPImageReader_h

#include <stdint.h>
#include "ImageDecoder.h"

namespace WebCore {

    // This class decodes a BMP image.  It is used as a base for the BMP and ICO
    // decoders, which wrap it in the appropriate code to read file headers, etc.
    class BMPImageReader : public ImageDecoder {
    public:
        BMPImageReader();

        // Does the actual decoding.  |data| starts at the beginning of the file,
        // but may be incomplete.
        virtual void decodeImage(SharedBuffer* data) = 0;

        // ImageDecoder
        virtual void setData(SharedBuffer* data, bool allDataReceived);
        virtual RGBA32Buffer* frameBufferAtIndex(size_t index);

    protected:
        enum AndMaskState {
            None,
            NotYetDecoded,
            Decoding,
        };

        // Decodes a single BMP, starting with an info header.
        void decodeBMP(SharedBuffer* data);

        // Read a value from |data[m_decodedOffset + additionalOffset]|, converting
        // from little to native endianness.
        inline uint16_t readUint16(SharedBuffer* data, int additionalOffset) const
        {
            uint16_t result;
            memcpy(&result, &data->data()[m_decodedOffset + additionalOffset], 2);
        #if PLATFORM(BIG_ENDIAN)
            result = ((result & 0xff) << 8) | ((result & 0xff00) >> 8);
        #endif
            return result;
        }

        inline uint32_t readUint32(SharedBuffer* data, int additionalOffset) const
        {
            uint32_t result;
            memcpy(&result, &data->data()[m_decodedOffset + additionalOffset], 4);
        #if PLATFORM(BIG_ENDIAN)
            result = ((result & 0xff) << 24) | ((result & 0xff00) << 8) |
                ((result & 0xff0000) >> 8) | ((result & 0xff000000) >> 24);
        #endif
            return result;
        }

        // An index into |m_data| representing how much we've already decoded.
        size_t m_decodedOffset;

        // The file offset at which the BMP info header starts.
        size_t m_headerOffset;

        // The file offset at which the actual image bits start.  When decoding ICO
        // files, this is set to 0, since it's not stored anywhere in a header; the
        // reader functions expect the image data to start immediately after the
        // header and (if necessary) color table.
        size_t m_imgDataOffset;

        // ICOs store a 1bpp "mask" immediately after the main bitmap image data
        // (and, confusingly, add its height to the biHeight value in the info
        // header, thus doubling it).  This variable tracks whether we have such a
        // mask and if we've started decoding it yet.
        AndMaskState m_andMaskState;

    private:
        // The various BMP compression types.  We don't currently decode all these.
        enum CompressionType {
            // Universal types
            RGB = 0,
            RLE8 = 1,
            RLE4 = 2,
            // Windows V3+ only
            BITFIELDS = 3,
            JPEG = 4,
            PNG = 5,
            // OS/2 2.x-only
            HUFFMAN1D,  // Stored in file as 3
            RLE24,      // Stored in file as 4
        };

        // These are based on the Windows BITMAPINFOHEADER and RGBTRIPLE structs,
        // but with unnecessary entries removed.
        struct BitmapInfoHeader {
            uint32_t biSize;
            int32_t biWidth;
            int32_t biHeight;
            uint16_t biBitCount;
            CompressionType biCompression;
            uint32_t biClrUsed;
        };
        struct RGBTriple {
            uint8_t rgbBlue;
            uint8_t rgbGreen;
            uint8_t rgbRed;
        };

        // Determines the size of the BMP info header.  Returns true if the size is
        // valid.
        bool getInfoHeaderSize(SharedBuffer* data);

        // Processes the BMP info header.  Returns true if the info header could be
        // decoded.
        bool processInfoHeader(SharedBuffer* data);

        // Helper function for processInfoHeader() which does the actual reading of
        // header values from the byte stream.  Returns false on error.
        bool readInfoHeader(SharedBuffer* data);

        // Returns true if this is a Windows V4+ BMP.
        inline bool isWindowsV4Plus() const
        {
            // Windows V4 info header is 108 bytes.  V5 is 124 bytes.
            return (m_infoHeader.biSize == 108) || (m_infoHeader.biSize == 124);
        }

        // Returns false if consistency errors are found in the info header.
        bool isInfoHeaderValid() const;

        // For BI_BITFIELDS images, initializes the m_bitMasks[] and m_bitOffsets[]
        // arrays.  processInfoHeader() will initialize these for other compression
        // types where needed.
        bool processBitmasks(SharedBuffer* data);

        // For paletted images, allocates and initializes the m_colorTable[] array.
        bool processColorTable(SharedBuffer* data);

        // Processes an RLE-encoded image.  Returns true if the entire image was
        // decoded.
        bool processRLEData(SharedBuffer* data);

        // Processes a set of non-RLE-compressed pixels.  Two cases:
        //   * inRLE = true: the data is inside an RLE-encoded bitmap.  Tries to
        //     process |numPixels| pixels on the current row; returns true on
        //     success.
        //   * inRLE = false: the data is inside a non-RLE-encoded bitmap.
        //     |numPixels| is ignored.  Expects |m_coord| to point at the beginning
        //     of the next row to be decoded.  Tries to process as many complete
        //     rows as possible.  Returns true if the whole image was decoded.
        bool processNonRLEData(SharedBuffer* data, bool inRLE, int numPixels);

        // Returns true if the current y-coordinate plus |numRows| would be past
        // the end of the image.  Here "plus" means "toward the end of the image",
        // so downwards for m_isTopDown images and upwards otherwise.
        inline bool pastEndOfImage(int numRows)
        {
            return m_isTopDown
                ? ((m_coord.y() + numRows) >= size().height())
                : ((m_coord.y() - numRows) < 0);
        }

        // Returns the pixel data for the current X coordinate in a uint32_t.
        // Assumes m_decodedOffset has been set to the beginning of the current
        // row.
        // NOTE: Only as many bytes of the return value as are needed to hold the
        // pixel data will actually be set.
        inline uint32_t readCurrentPixel(SharedBuffer* data, int bytesPerPixel) const
        {
            const int additionalOffset = m_coord.x() * bytesPerPixel;
            switch (bytesPerPixel) {
            case 2:
                return readUint16(data, additionalOffset);

            case 3: {
                // It doesn't matter that we never set the most significant byte of
                // the return value here in little-endian mode, the caller won't
                // read it.
                uint32_t pixel;
                memcpy(&pixel,
                       &data->data()[m_decodedOffset + additionalOffset], 3);
        #if PLATFORM(BIG_ENDIAN)
                pixel = ((pixel & 0xff00) << 8) | ((pixel & 0xff0000) >> 8) |
                    ((pixel & 0xff000000) >> 24);
        #endif
                return pixel;
            }

            case 4:
                return readUint32(data, additionalOffset);

            default:
                ASSERT_NOT_REACHED();
                return 0;
            }
        }

        // Returns the value of the desired component (0, 1, 2, 3 == R, G, B, A) in
        // the given pixel data.
        inline unsigned getComponent(uint32_t pixel, int component) const
        {
            return ((pixel & m_bitMasks[component]) >> m_bitShiftsRight[component])
                << m_bitShiftsLeft[component];
        }

        inline unsigned getAlpha(uint32_t pixel) const
        {
            // For images without alpha, return alpha of 0xff.
            if (m_bitMasks[3] == 0)
                return 0xff;

            return getComponent(pixel, 3);
        }

        // Sets the current pixel to the color given by |colorIndex|.  This also
        // increments the relevant local variables to move the current pixel right
        // by one.
        inline void setI(size_t colorIndex)
        {
            setRGBA(m_colorTable[colorIndex].rgbRed, m_colorTable[colorIndex].rgbGreen,
                    m_colorTable[colorIndex].rgbBlue, 0xff);
        }

        // Like setI(), but with the individual component values specified.
        inline void setRGBA(unsigned red, unsigned green, unsigned blue, unsigned alpha)
        {
            RGBA32Buffer::setRGBA(
                m_frameBufferCache.first().bitmap().getAddr32(m_coord.x(), m_coord.y()),
                red, green, blue, alpha);
            m_coord.move(1, 0);
        }

        // Fills pixels from the current X-coordinate up to, but not including,
        // |endCoord| with the color given by the individual components.  This also
        // increments the relevant local variables to move the current pixel right
        // to |endCoord|.
        inline void fillRGBA(int endCoord, unsigned red, unsigned green, unsigned blue, unsigned alpha)
        {
            while (m_coord.x() < endCoord)
                setRGBA(red, green, blue, alpha);
        }

        // Resets the relevant local variables to start drawing at the left edge of
        // the "next" row, where "next" is above or below the current row depending
        // on the value of |m_isTopDown|.
        void moveBufferToNextRow();

        // The BMP info header.
        BitmapInfoHeader m_infoHeader;

        // True if this is an OS/2 1.x (aka Windows 2.x) BMP.  The struct layouts
        // for this type of BMP are slightly different from the later, more common
        // formats.
        bool m_isOS21x;

        // True if this is an OS/2 2.x BMP.  The meanings of compression types 3
        // and 4 for this type of BMP differ from Windows V3+ BMPs.
        //
        // This will be falsely negative in some cases, but only ones where the way
        // we misinterpret the data is irrelevant.
        bool m_isOS22x;

        // True if the BMP is not vertically flipped, that is, the first line of
        // raster data in the file is the top line of the image.
        bool m_isTopDown;

        // These flags get set to false as we finish each processing stage.
        bool m_needToProcessBitmasks;
        bool m_needToProcessColorTable;

        // Masks/offsets for the color values for non-palette formats.  These are
        // bitwise, with array entries 0, 1, 2, 3 corresponding to R, G, B, A.
        //
        // The right/left shift values are meant to be applied after the masks.
        // We need to right shift to compensate for the bitfields' offsets into the
        // 32 bits of pixel data, and left shift to scale the color values up for
        // fields with less than 8 bits of precision.  Sadly, we can't just combine
        // these into one shift value because the net shift amount could go either
        // direction.  (If only "<< -x" were equivalent to ">> x"...)
        uint32_t m_bitMasks[4];
        int m_bitShiftsRight[4];
        int m_bitShiftsLeft[4];

        // The color palette, for paletted formats.
        size_t m_tableSizeInBytes;
        Vector<RGBTriple> m_colorTable;

        // The coordinate to which we've decoded the image.
        IntPoint m_coord;

        // Variables that track whether we've seen pixels with alpha values != 0
        // and == 0, respectively.  See comments in processNonRLEData() on how
        // these are used.
        bool m_seenNonZeroAlphaPixel;
        bool m_seenZeroAlphaPixel;
    };

} // namespace WebCore

#endif
