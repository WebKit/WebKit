/*
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
#include "config.h"
#include "KURL.h"
#include "CString.h"
#include "TextEncoding.h"

#include "NotImplemented.h"
#include "qurl.h"

namespace WebCore {

#if QT_VERSION < 0x040500
static const char hexnumbers[] = "0123456789ABCDEF";
static inline char toHex(char c)
{
    return hexnumbers[c & 0xf];
}
#endif

KURL::KURL(const QUrl& url)
{
    *this = KURL(KURL(), url.toEncoded().constData(), UTF8Encoding());
}

KURL::operator QUrl() const
{
#if QT_VERSION < 0x040500
    unsigned length = m_string.length();

    QByteArray ba;
    ba.reserve(length);

    int path = -1;
    int host = m_string.find("://");
    if (host != -1) {
        host += 3;

        path = m_string.find('/', host);
    }

    for (unsigned i = 0; i < length; ++i) {
        const char chr = static_cast<char>(m_string[i]);

        switch (chr) {
            encode:
            case '{':
            case '}':
            case '|':
            case '\\':
            case '^':
            case '`':
                ba.append('%');
                ba.append(toHex((chr & 0xf0) >> 4));
                ba.append(toHex(chr & 0xf));
                break;
            case '[':
            case ']':
                // special case: if this is the host part, don't encode
                // otherwise, encode
                if (host == -1 || (path != -1 && i >= path))
                    goto encode;
                // fall through
            default:
                ba.append(chr);
                break;
        }
    }
#else
    // Qt 4.5 or later
    // No need for special encoding
    QString str = QString::fromRawData(reinterpret_cast<const QChar*>(m_string.characters()), m_string.length());
    QByteArray ba = str.toUtf8();
#endif

    QUrl url = QUrl::fromEncoded(ba);
    return url;
}

String KURL::fileSystemPath() const
{
    if (!isValid() || !protocolIs("file"))
        return String();

    return String(path());
}

}

