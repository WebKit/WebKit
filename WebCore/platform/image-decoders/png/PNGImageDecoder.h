/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PNGImageDecoder_h
#define PNGImageDecoder_h

#include "ImageDecoder.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

    class PNGImageReader;

    // This class decodes the PNG image format.
    class PNGImageDecoder : public ImageDecoder {
    public:
        PNGImageDecoder();
        ~PNGImageDecoder();

        virtual String filenameExtension() const { return "png"; }

        // Take the data and store it.
        virtual void setData(SharedBuffer* data, bool allDataReceived);

        // Whether or not the size information has been decoded yet.
        virtual bool isSizeAvailable();

        virtual RGBA32Buffer* frameBufferAtIndex(size_t index);

        void decode(bool sizeOnly = false);

        // Callbacks from libpng
        void decodingFailed();
        void headerAvailable();
        void rowAvailable(unsigned char* rowBuffer, unsigned rowIndex, int interlacePass);
        void pngComplete();

    private:
        OwnPtr<PNGImageReader> m_reader;
    };

} // namespace WebCore

#endif
