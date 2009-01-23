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

#include "config.h"
#include "BMPImageDecoder.h"

namespace WebCore {

// Number of bits in .BMP used to store the file header (doesn't match
// "sizeof(BMPImageDecoder::BitmapFileHeader)" since we omit some fields and
// don't pack).
static const size_t sizeOfFileHeader = 14;

void BMPImageDecoder::decodeImage(SharedBuffer* data)
{
    // Read and process file header.
    if ((m_decodedOffset < sizeOfFileHeader) && !processFileHeader(data))
        return;

    // Decode BMP.
    decodeBMP(data);
}

bool BMPImageDecoder::processFileHeader(SharedBuffer* data)
{
    // Read file header.
    ASSERT(!m_decodedOffset);
    if (data->size() < sizeOfFileHeader)
        return false;
    const uint16_t fileType =
        (data->data()[0] << 8) | static_cast<uint8_t>(data->data()[1]);
    m_imgDataOffset = readUint32(data, 10);
    m_decodedOffset = m_headerOffset = sizeOfFileHeader;

    // See if this is a bitmap filetype we understand.
    enum {
        BMAP = 'BM',
        // The following additional OS/2 2.x header values (see
        // http://www.fileformat.info/format/os2bmp/egff.htm ) aren't widely
        // decoded, and are unlikely to be in much use.
        /*
        ICON = 'IC',
        POINTER = 'PT',
        COLORICON = 'CI',
        COLORPOINTER = 'CP',
        BITMAPARRAY = 'BA',
        */
    };
    if (fileType != BMAP)
        m_failed = true;

    return !m_failed;
}

} // namespace WebCore
