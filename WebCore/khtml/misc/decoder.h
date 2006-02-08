/*
    This file is part of the KDE libraries

    Copyright (C) 1999 Lars Knoll (knoll@mpi-hd.mpg.de)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

*/
#ifndef KHTMLDECODER_H
#define KHTMLDECODER_H

#include <QString.h>
class QTextCodec;
class QTextDecoder;

namespace khtml {
/**
 * @internal
 */
class Decoder : public Shared<Decoder>
{
public:
    enum EncodingType {
        DefaultEncoding,
        AutoDetectedEncoding,
        EncodingFromXMLHeader,
        EncodingFromMetaTag,
        EncodingFromHTTPHeader,
        UserChosenEncoding
    };
    
    Decoder();
    ~Decoder();

    void setEncoding(const char *encoding, EncodingType type);
    const char *encoding() const;

    QString decode(const char *data, int len);

    bool visuallyOrdered() const { return visualRTL; }

    const QTextCodec *codec() const { return m_codec; }

    QString flush() const;

protected:
    // codec used for decoding. default is Latin1.
    QTextCodec *m_codec;
    QTextDecoder *m_decoder;
    QCString enc;
    EncodingType m_type;

    // Our version of QString works well for all-8-bit characters, and allows null characters.
    // This works better than QCString when there are null characters involved.
    QString buffer;

    bool body;
    bool beginning;
    bool visualRTL;
};

}

#endif
