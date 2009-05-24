/*
 * Copyright (C) 2006 Lars Knoll <lars@trolltech.com>
 * Copyright (C) 2008 Holger Hans Peter Freyther
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

#include "config.h"
#include "TextCodecQt.h"
#include "PlatformString.h"
#include "CString.h"
#include <qset.h>
// #include <QDebug>

namespace WebCore {

static QSet<QByteArray> *unique_names = 0;

static const char *getAtomicName(const QByteArray &name)
{
    if (!unique_names)
        unique_names = new QSet<QByteArray>;

    unique_names->insert(name);
    return unique_names->find(name)->constData();
}

void TextCodecQt::registerEncodingNames(EncodingNameRegistrar registrar)
{
    QList<int> mibs = QTextCodec::availableMibs();
//     qDebug() << ">>>>>>>>> registerEncodingNames";

    for (int i = 0; i < mibs.size(); ++i) {
        QTextCodec *c = QTextCodec::codecForMib(mibs.at(i));
        const char *name = getAtomicName(c->name());
        registrar(name, name);
//         qDebug() << "    " << name << name;
        QList<QByteArray> aliases = c->aliases();
        for (int i = 0; i < aliases.size(); ++i) {
            const char *a = getAtomicName(aliases.at(i));
//             qDebug() << "     (a) " << a << name;
            registrar(a, name);
        }
    }
}

static PassOwnPtr<TextCodec> newTextCodecQt(const TextEncoding& encoding, const void*)
{
    return new TextCodecQt(encoding);
}

void TextCodecQt::registerCodecs(TextCodecRegistrar registrar)
{
    QList<int> mibs = QTextCodec::availableMibs();
//     qDebug() << ">>>>>>>>> registerCodecs";

    for (int i = 0; i < mibs.size(); ++i) {
        QTextCodec *c = QTextCodec::codecForMib(mibs.at(i));
        const char *name = getAtomicName(c->name());
//         qDebug() << "    " << name;
        registrar(name, newTextCodecQt, 0);
    }
}

TextCodecQt::TextCodecQt(const TextEncoding& encoding)
    : m_encoding(encoding)
{
    m_codec = QTextCodec::codecForName(m_encoding.name());
}

TextCodecQt::~TextCodecQt()
{
}


String TextCodecQt::decode(const char* bytes, size_t length, bool flush, bool /*stopOnError*/, bool& sawError)
{
    QString unicode = m_codec->toUnicode(bytes, length, &m_state);
    sawError = m_state.invalidChars != 0;

    if (flush) {
        m_state.flags = QTextCodec::DefaultConversion;
        m_state.remainingChars = 0;
        m_state.invalidChars = 0;
    }

    return unicode;
}

CString TextCodecQt::encode(const UChar* characters, size_t length, UnencodableHandling)
{
    if (!length)
        return "";

    // FIXME: do something sensible with UnencodableHandling

    QByteArray ba = m_codec->fromUnicode(reinterpret_cast<const QChar*>(characters), length, 0);
    return CString(ba.constData(), ba.length());
}


} // namespace WebCore
