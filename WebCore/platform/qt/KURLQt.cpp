/*
 * Copyright (C) 2007 Trolltech ASA
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

#include "NotImplemented.h"
#include "qurl.h"

namespace WebCore {

static const char hexnumbers[] = "0123456789ABCDEF";
static inline char toHex(char c)
{
    return hexnumbers[c & 0xf];
}

KURL::KURL(const QUrl& url)
{
    *this = KURL(url.toEncoded().constData());
}

KURL::operator QUrl() const
{
    QByteArray ba;
    ba.reserve(urlString.length());

    for (const char *src = urlString.ascii(); *src; ++src) {
        const char chr = *src;

        switch (chr) {
            case '{':
            case '}':
            case '|':
            case '\\':
            case '^':
            case '[':
            case ']':
            case '`':
                ba.append('%');
                ba.append(toHex((chr & 0xf0) >> 4));
                ba.append(toHex(chr & 0xf));
                break;
            default:
                ba.append(chr);
                break;
        }
    }

    QUrl url = QUrl::fromEncoded(ba);
    return url;
}

String KURL::fileSystemPath() const
{
    notImplemented();
    return String();
}

}

