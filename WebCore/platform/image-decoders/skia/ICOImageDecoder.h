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
#include "PNGImageDecoder.h"

namespace WebCore {

    // This class decodes the ICO and CUR image formats.
    class ICOImageDecoder : public BMPImageReader {
    public:
        // See comments on |m_preferredIconSize| below.
        ICOImageDecoder(const IntSize& preferredIconSize)
            : m_preferredIconSize(preferredIconSize)
            , m_imageType(Unknown)
        {
            m_andMaskState = NotYetDecoded;
        }

        virtual String filenameExtension() const { return "ico"; }

        // BMPImageReader
        virtual void decodeImage(SharedBuffer* data);
        virtual RGBA32Buffer* frameBufferAtIndex(size_t index);

        // ImageDecoder
        virtual bool isSizeAvailable() const;
        virtual IntSize size() const;

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

        // Processes the ICONDIR at the beginning of the data.  Returns true if the
        // directory could be decoded.
        bool processDirectory(SharedBuffer*);

        // Processes the ICONDIRENTRY records after the directory.  Keeps the
        // "best" entry as the one we'll decode.  Returns true if the entries could
        // be decoded.
        bool processDirectoryEntries(SharedBuffer*);

        // Reads and returns a directory entry from the current offset into |data|.
        IconDirectoryEntry readDirectoryEntry(SharedBuffer*);

        // Returns true if |entry| is a preferable icon entry to m_dirEntry.
        // Larger sizes, or greater bitdepths at the same size, are preferable.
        bool isBetterEntry(const IconDirectoryEntry&) const;

        // Called when the image to be decoded is a PNG rather than a BMP.
        // Instantiates a PNGImageDecoder, decodes the image, and copies the
        // results locally.
        void decodePNG(SharedBuffer*);

        // The entry size we should prefer.  If this is empty, we choose the
        // largest available size.  If no entries of the desired size are
        // available, we pick the next larger size.
        IntSize m_preferredIconSize;

        // The headers for the ICO.
        IconDirectory m_directory;
        IconDirectoryEntry m_dirEntry;

        // The PNG decoder, if we need to use one.
        PNGImageDecoder m_pngDecoder;

        // What kind of image data is stored at the entry we're decoding.
        ImageType m_imageType;
    };

} // namespace WebCore

#endif
