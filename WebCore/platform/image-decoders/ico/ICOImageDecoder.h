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

#ifndef ICOImageDecoder_h
#define ICOImageDecoder_h

#include "BMPImageReader.h"

namespace WebCore {

    class PNGImageDecoder;

    // This class decodes the ICO and CUR image formats.
    class ICOImageDecoder : public ImageDecoder {
    public:
        // See comments on |m_preferredIconSize| below.
        ICOImageDecoder(const IntSize& preferredIconSize);

        // ImageDecoder
        virtual String filenameExtension() const { return "ico"; }
        virtual void setData(SharedBuffer*, bool allDataReceived);
        virtual bool isSizeAvailable();
        virtual IntSize size() const;
        virtual RGBA32Buffer* frameBufferAtIndex(size_t index);

    private:
        enum ImageType {
            Unknown,
            BMP,
            PNG,
        };

        // These are based on the Windows ICONDIR and ICONDIRENTRY structs, but
        // with unnecessary entries removed.
        struct IconDirectory {
            uint16_t idCount;
        };
        struct IconDirectoryEntry {
            uint16_t bWidth;   // 16 bits so we can represent 256 as 256, not 0
            uint16_t bHeight;  // "
            uint16_t wBitCount;
            uint32_t dwImageOffset;
        };

        inline uint16_t readUint16(int offset) const
        {
            return BMPImageReader::readUint16(m_data.get(),
                                              m_decodedOffset + offset);
        }

        inline uint32_t readUint32(int offset) const
        {
            return BMPImageReader::readUint32(m_data.get(),
                                              m_decodedOffset + offset);
        }

        // Decodes the image.  If |onlySize| is true, stops decoding after
        // calculating the image size.  If decoding fails but there is no more
        // data coming, sets the "decode failure" flag.
        //
        // NOTE: If the desired entry is a PNG, this doesn't actually trigger
        // decoding, it merely ensures the decoder is created and ready to
        // decode.  The caller will then call a function on the PNGImageDecoder
        // that actually triggers decoding.
        void decodeWithCheckForDataEnded(bool onlySize);

        // Decodes the image.  If |onlySize| is true, stops decoding after
        // calculating the image size.  Returns whether decoding succeeded.
        // NOTE: Used internally by decodeWithCheckForDataEnded().  Other people
        // should not call this.
        bool decode(bool onlySize);

        // Processes the ICONDIR at the beginning of the data.  Returns true if
        // the directory could be decoded.
        bool processDirectory();

        // Processes the ICONDIRENTRY records after the directory.  Keeps the
        // "best" entry as the one we'll decode.  Returns true if the entries
        // could be decoded.
        bool processDirectoryEntries();

        // Reads and returns a directory entry from the current offset into
        // |data|.
        IconDirectoryEntry readDirectoryEntry();

        // Returns true if |entry| is a preferable icon entry to m_dirEntry.
        // Larger sizes, or greater bitdepths at the same size, are preferable.
        bool isBetterEntry(const IconDirectoryEntry&) const;

        // Determines whether the desired entry is a BMP or PNG.  Returns true
        // if the type could be determined.
        bool processImageType();

        // True if we've seen all the data.
        bool m_allDataReceived;

        // An index into |m_data| representing how much we've already decoded.
        // Note that this only tracks data _this_ class decodes; once the
        // BMPImageReader takes over this will not be updated further.
        size_t m_decodedOffset;

        // The entry size we should prefer.  If this is empty, we choose the
        // largest available size.  If no entries of the desired size are
        // available, we pick the next larger size.
        IntSize m_preferredIconSize;

        // The headers for the ICO.
        IconDirectory m_directory;
        IconDirectoryEntry m_dirEntry;

        // The BMP reader, if we need to use one.
        OwnPtr<BMPImageReader> m_bmpReader;

        // The PNG decoder, if we need to use one.
        OwnPtr<PNGImageDecoder> m_pngDecoder;

        // What kind of image data is stored at the entry we're decoding.
        ImageType m_imageType;
    };

} // namespace WebCore

#endif
