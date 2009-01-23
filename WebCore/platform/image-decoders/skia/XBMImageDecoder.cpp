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
#include "XBMImageDecoder.h"

#include <algorithm>

namespace WebCore {

XBMImageDecoder::XBMImageDecoder()
    : m_decodeOffset(0)
    , m_allDataReceived(false)
    , m_decodedHeader(false)
    , m_dataType(Unknown)
    , m_bitsDecoded(0)
{
}

void XBMImageDecoder::setData(SharedBuffer* data, bool allDataReceived)
{
    ImageDecoder::setData(data, allDataReceived);

    const Vector<char>& buf = m_data->buffer();
    if (buf.size() > m_xbmString.size())
        m_xbmString.append(&buf[m_xbmString.size()], buf.size() - m_xbmString.size());

    m_allDataReceived = allDataReceived;
}

bool XBMImageDecoder::isSizeAvailable() const
{
    // This method should either (a) not be const, or (b) not be expected to
    // do anything that changes m_sizeAvailable. The png and jpeg decoders
    // get around this with callbacks from external libraries.
    //
    // FIXME: Find out if we can patch webkit to take care of this.
    if (!ImageDecoder::isSizeAvailable() && !m_failed)
        const_cast<XBMImageDecoder*>(this)->decodeXBM(true);

    return !m_failed && ImageDecoder::isSizeAvailable();
}

RGBA32Buffer* XBMImageDecoder::frameBufferAtIndex(size_t index)
{
    // Allocate a framebuffer if necessary. New framebuffers have their status
    // initialized to RGBA32Buffer::FrameEmpty.
    if (m_frameBufferCache.isEmpty())
        m_frameBufferCache.resize(1);

    RGBA32Buffer& frame = m_frameBufferCache[0];

    // Attempt to get the size if we don't have it yet.
    if (!ImageDecoder::isSizeAvailable())
        decodeXBM(true);
    
    // Size the framebuffer once we know the right size.
    if (ImageDecoder::isSizeAvailable() &&
        frame.status() == RGBA32Buffer::FrameEmpty) {
        if (!frame.setSize(size().width(), size().height())) {
            m_failed = true;
            frame.setStatus(RGBA32Buffer::FrameComplete);
            return 0;
        }
        frame.setStatus(RGBA32Buffer::FramePartial);
    }
    
    // Keep trying to decode until we've got the entire image.
    if (frame.status() != RGBA32Buffer::FrameComplete)
        decodeXBM(false);

    return &frame;
}

bool XBMImageDecoder::decodeHeader()
{
    ASSERT(m_decodeOffset <= m_xbmString.size());
    ASSERT(!m_decodedHeader);

    const char* input = m_xbmString.c_str();

    // At least 2 "#define <string> <unsigned>" sequences are required. These
    // specify the width and height of the image.
    int width, height;
    if (!ImageDecoder::isSizeAvailable()) {
        int count;
        if (sscanf(&input[m_decodeOffset], "#define %*s %i #define %*s %i%n",
                   &width, &height, &count) != 2)
            return false;

        // The width and height need to follow some rules.
        if (width < 0 || width > maxDimension || height < 0 || height > maxDimension) {
            // If this happens, decoding should not continue.
            setFailed();
            return false;
        }

        if (!setSize(width, height)) {
            setFailed();
            return false;
        }
        m_decodeOffset += count;
        ASSERT(m_decodeOffset <= m_xbmString.size());
    }

    ASSERT(ImageDecoder::isSizeAvailable());

    // Now we're looking for something that tells us that we've seen all of the
    // "#define <string> <unsigned>" sequences that we're going to. Mozilla
    // just looks for " char " or " short ". We'll do the same.
    if (m_dataType == Unknown) {
        const char* x11hint = " char ";
        const char* x11HintLocation = strstr(&input[m_decodeOffset], x11hint);
        if (x11HintLocation) {
            m_dataType = X11;
            m_decodeOffset += ((x11HintLocation - &input[m_decodeOffset]) + strlen(x11hint));
        } else {
            const char* x10hint = " short ";
            const char* x10HintLocation = strstr(&input[m_decodeOffset], x10hint);
            if (x10HintLocation) {
                m_dataType = X10;
                m_decodeOffset += ((x10HintLocation - &input[m_decodeOffset]) + strlen(x10hint));
            } else
                return false;
        }
        ASSERT(m_decodeOffset <= m_xbmString.size());
    }

    // Find the start of the data. Again, we do what mozilla does and just
    // look for a '{' in the input.
    const char* found = strchr(&input[m_decodeOffset], '{');
    if (!found)
        return false;

    // Advance to character after the '{'
    m_decodeOffset += ((found - &input[m_decodeOffset]) + 1);
    ASSERT(m_decodeOffset <= m_xbmString.size());
    m_decodedHeader = true;

    return true;
}

// The data in an XBM file is provided as an array of either "char" or "short"
// values. These values are decoded one at a time using strtoul() and the bits
// are used to set the alpha value for the image.
//
// The value for the color is always set to RGB(0,0,0), the alpha value takes
// care of whether or not the pixel shows up.
//
// Since the data may arrive in chunks, and many prefixes of valid numbers are
// themselves valid numbers, this code needs to check to make sure that the
// value is not truncated. This is done by consuming space after the value
// read until a ',' or a '}' occurs. In a valid XBM, one of these characters
// will occur after each value.
//
// The checks after strtoul are based on Mozilla's nsXBMDecoder.cpp.
bool XBMImageDecoder::decodeDatum(uint16_t* result)
{
    const char* input = m_xbmString.c_str();
    char* endPtr;
    const uint16_t value = strtoul(&input[m_decodeOffset], &endPtr, 0);

    // End of input or couldn't decode anything, can't go any further.
    if (endPtr == &input[m_decodeOffset] || !*endPtr)
        return false;

    // Possibly a hex value truncated at "0x". Need more data.
    if (value == 0 && (*endPtr == 'x' || *endPtr == 'X'))
        return false;

    // Skip whitespace
    while (*endPtr && isspace(*endPtr))
        ++endPtr;

    // Out of input, don't know what comes next.
    if (!*endPtr)
        return false;

    // If the next non-whitespace character is not one of these, it's an error.
    // Every valid entry in the data array needs to be followed by ',' or '}'.
    if (*endPtr != ',' && *endPtr != '}') {
        setFailed();
        return false;
    }

    // At this point we have a value.
    *result = value;

    // Skip over the decoded value plus the delimiter (',' or '}').
    m_decodeOffset += ((endPtr - &input[m_decodeOffset]) + 1);
    ASSERT(m_decodeOffset <= m_xbmString.size());

    return true;
}

bool XBMImageDecoder::decodeData()
{
    ASSERT(m_decodeOffset <= m_xbmString.size());
    ASSERT(m_decodedHeader && !m_frameBufferCache.isEmpty());

    RGBA32Buffer& frame = m_frameBufferCache[0];

    ASSERT(frame.status() == RGBA32Buffer::FramePartial);

    const int bitsPerRow = size().width();

    ASSERT(m_dataType != Unknown);

    while (m_bitsDecoded < (size().width() * size().height())) {
        uint16_t value;
        if (!decodeDatum(&value))
            return false;

        int x = m_bitsDecoded % bitsPerRow;
        const int y = m_bitsDecoded / bitsPerRow;

        // How many bits will be written?
        const int bits = std::min(bitsPerRow - x, (m_dataType == X11) ? 8 : 16);

        // Only the alpha channel matters here, so the color values are always
        // set to 0.
        for (int i = 0; i < bits; ++i)
            frame.setRGBA(x++, y, 0, 0, 0, value & (1 << i) ? 255 : 0);

        m_bitsDecoded += bits;
    }

    frame.setStatus(RGBA32Buffer::FrameComplete);

    return true;
}

// Decode as much as we can of the XBM file.
void XBMImageDecoder::decodeXBM(bool sizeOnly)
{
    if (failed())
        return;

    bool decodeResult = false;

    if (!m_decodedHeader)
        decodeResult = decodeHeader();

    if (m_decodedHeader && !sizeOnly)
        decodeResult = decodeData();

    // The header or the data could not be decoded, but there is no more
    // data: decoding has failed.
    if (!decodeResult && m_allDataReceived)
        setFailed();
}

} // namespace WebCore
