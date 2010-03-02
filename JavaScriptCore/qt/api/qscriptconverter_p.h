/*
    Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)

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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef qscriptconverter_p_h
#define qscriptconverter_p_h

#include <JavaScriptCore/JavaScript.h>
#include <QtCore/qnumeric.h>
#include <QtCore/qstring.h>

extern char *qdtoa(double d, int mode, int ndigits, int *decpt, int *sign, char **rve, char **digits_str);

/*
  \internal
  \class QScriptConverter
  QScriptValue and QScriptEngine helper class. This class's responsibility is to convert values
  between JS values and Qt/C++ values.

  This is a nice way to inline these functions in both QScriptValue and QScriptEngine.
*/
class QScriptConverter {
public:
    static quint32 toArrayIndex(const JSStringRef jsstring)
    {
        // FIXME this function should be exported by JSC C API.
        QString qstring = toString(jsstring);

        bool ok;
        quint32 idx = qstring.toUInt(&ok);
        if (!ok || toString(idx) != qstring)
            idx = 0xffffffff;

        return idx;
    }

    static QString toString(const JSStringRef str)
    {
        return QString(reinterpret_cast<const QChar*>(JSStringGetCharactersPtr(str)), JSStringGetLength(str));
    }
    static JSStringRef toString(const QString& str)
    {
        return JSStringCreateWithUTF8CString(str.toUtf8().constData());
    }
    static JSStringRef toString(const char* str)
    {
        return JSStringCreateWithUTF8CString(str);
    }
    static QString toString(double value)
    {
        // FIXME this should be easier. The ideal fix is to create
        // a new function in JSC C API which could cover the functionality.

        if (qIsNaN(value))
            return QString::fromLatin1("NaN");
        if (qIsInf(value))
            return QString::fromLatin1(value < 0 ? "-Infinity" : "Infinity");
        if (!value)
            return QString::fromLatin1("0");

        QByteArray buf;
        buf.reserve(24);
        int decpt;
        int sign;
        char* result = 0;
        (void)qdtoa(value, 0, 0, &decpt, &sign, 0, &result);

        if (!result)
            return QString();

        if (decpt <= 0 && decpt > -6) {
            buf.fill('0', -decpt + 2 + sign);
            if (sign) // fix the sign.
                buf[0] = '-';
            buf[sign + 1] = '.';
            buf += result;
        } else {
            if (sign)
                buf += '-';
            buf += result;
            int length = buf.length() - sign;
            if (decpt <= 21 && decpt > 0) {
                if (length <= decpt)
                    buf += QByteArray().fill('0', decpt - length);
                else
                    buf.insert(decpt + sign, '.');
            } else if (result[0] >= '0' && result[0] <= '9') {
                if (length > 1)
                    buf.insert(1 + sign, '.');
                buf += 'e';
                buf += (decpt >= 0) ? '+' : '-';
                int e = decpt - 1;
                if (e < 0)
                    e = -e;
                if (e >= 100)
                    buf += '0' + e / 100;
                if (e >= 10)
                    buf += '0' + (e % 100) / 10;
                buf += '0' + e % 10;
            }
        }
        free(result);
        return QString::fromLatin1(buf);
    }
};

#endif // qscriptconverter_p_h
