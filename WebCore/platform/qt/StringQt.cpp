/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "PlatformString.h"
#include "DeprecatedString.h"

#include <QString>

namespace WebCore {

// String conversions
String::String(const QString& qstr)
{
    unsigned int len = qstr.length();
    const UChar* str = reinterpret_cast<const UChar*>(qstr.constData());

    if (!str)
        return;
    
    if (len == 0)
        m_impl = StringImpl::empty();
    else
        m_impl = new StringImpl(str, len);
}

String::operator QString() const
{
    return QString(reinterpret_cast<const QChar*>(characters()), length());
}

// DeprecatedString conversions
DeprecatedString::DeprecatedString(const QString& qstr)
{
    if (qstr.isNull()) {
        (*this) = DeprecatedString::null;
    } else {
        QByteArray utf8Data = qstr.toUtf8();
        (*this) = DeprecatedString::fromUtf8(utf8Data.data(), utf8Data.length());
    }
}

DeprecatedString::operator QString() const
{
    return QString(reinterpret_cast<const QChar*>(unicode()), length());
}

}

// vim: ts=4 sw=4 et
