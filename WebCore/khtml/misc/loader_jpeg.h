/*
    This file is part of the KDE libraries

    Copyright (C) 2000 Dirk Mueller (mueller@kde.org)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
    AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
    AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
    CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

    This is a helper for progressive loading of JPEG's. 
*/

#ifndef _khtml_loader_jpeg_h
#define _khtml_loader_jpeg_h

#include <qasyncimageio.h>

namespace khtml
{
    /**
     * @internal
     *
     * An incremental loader factory for JPEG's.
     */
    class KJPEGFormatType : public QImageFormatType
    {
    public:
        QImageFormat* decoderFor(const uchar* buffer, int length);
        const char* formatName() const;
    };

};


// -----------------------------------------------------------------------------

#endif // _khtml_loader_jpeg_h
