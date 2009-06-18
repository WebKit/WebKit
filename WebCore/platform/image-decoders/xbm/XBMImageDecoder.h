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

#ifndef XBMImageDecoder_h
#define XBMImageDecoder_h

#include "ImageDecoder.h"

namespace WebCore {

    // This class decodes the XBM image format.
    class XBMImageDecoder : public ImageDecoder {
    public:
        XBMImageDecoder();
        virtual ~XBMImageDecoder() {}

        virtual String filenameExtension() const { return "xbm"; }

        virtual void setData(SharedBuffer* data, bool allDataReceived);
        // Whether or not the size information has been decoded yet.
        virtual bool isSizeAvailable();
        virtual RGBA32Buffer* frameBufferAtIndex(size_t index);

    private:
        // Restricts image size to something "reasonable".
        // This protects agains ridiculously large XBMs and prevents bad things
        // like overflow of m_bitsDecoded.
        static const int maxDimension = 65535;

        // In X10, an array of type "short" is used to declare the image bits,
        // but in X11, the type is "char".
        enum DataType {
            Unknown,
            X10,
            X11,
        };

        bool decodeHeader();
        bool decodeDatum(uint16_t* result);
        bool decodeData();
        void decode(bool sizeOnly);

        // FIXME: Copying all the XBM data just so we can NULL-terminate, just
        // so we can use sscanf() and friends, is lame.  The decoder should be
        // rewritten to operate on m_data directly.
        Vector<char> m_xbmString;  // Null-terminated copy of the XBM data.
        size_t m_decodeOffset;    // The current offset in m_xbmString for decoding.
        bool m_allDataReceived;
        bool m_decodedHeader;
        enum DataType m_dataType;
        int m_bitsDecoded;
    };

} // namespace WebCore

#endif
