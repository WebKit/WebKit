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

    $Id$
*/
#ifndef KHTMLDECODER_H
#define KHTMLDECODER_H

#include <qstring.h>
class QTextCodec;
class QTextDecoder;

namespace khtml {
/**
 * @internal
 */
class Decoder
{
public:
    Decoder();
    ~Decoder();

    void setEncoding(const char *encoding, bool force = false);
    const char *encoding() const;

    QString decode(const char *data, int len);

    bool visuallyOrdered() const { return visualRTL; }

    const QTextCodec *codec() const { return m_codec; }

    QString flush() const;

protected:
    // codec used for decoding. default is Latin1.
    QTextCodec *m_codec;
    QTextDecoder *m_decoder; // only used for utf16
    QCString enc;

    QCString buffer;

    bool body;
    bool beginning;
    bool visualRTL;
    bool haveEncoding;
};

};
#endif
