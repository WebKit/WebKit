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

#include "qscriptvalue.h"
#include <JavaScriptCore/JavaScript.h>
#include <QtCore/qglobal.h>
#include <QtCore/qnumeric.h>
#include <QtCore/qstring.h>
#include <QtCore/qvarlengtharray.h>

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

        QVarLengthArray<char, 25> buf;
        int decpt;
        int sign;
        char* result = 0;
        char* endresult;
        (void)qdtoa(value, 0, 0, &decpt, &sign, &endresult, &result);

        if (!result)
            return QString();

        int resultLen = endresult - result;
        if (decpt <= 0 && decpt > -6) {
            buf.resize(-decpt + 2 + sign);
            qMemSet(buf.data(), '0', -decpt + 2 + sign);
            if (sign) // fix the sign.
                buf[0] = '-';
            buf[sign + 1] = '.';
            buf.append(result, resultLen);
        } else {
            if (sign)
                buf.append('-');
            int length = buf.size() - sign + resultLen;
            if (decpt <= 21 && decpt > 0) {
                if (length <= decpt) {
                    const char* zeros = "0000000000000000000000000";
                    buf.append(result, resultLen);
                    buf.append(zeros, decpt - length);
                } else {
                    buf.append(result, decpt);
                    buf.append('.');
                    buf.append(result + decpt, resultLen - decpt);
                }
            } else if (result[0] >= '0' && result[0] <= '9') {
                if (length > 1) {
                    buf.append(result, 1);
                    buf.append('.');
                    buf.append(result + 1, resultLen - 1);
                } else
                    buf.append(result, resultLen);
                buf.append('e');
                buf.append(decpt >= 0 ? '+' : '-');
                int e = qAbs(decpt - 1);
                if (e >= 100)
                    buf.append('0' + e / 100);
                if (e >= 10)
                    buf.append('0' + (e % 100) / 10);
                buf.append('0' + e % 10);
            }
        }
        free(result);
        buf.append(0);
        return QString::fromLatin1(buf.constData());
    }

    static JSPropertyAttributes toPropertyFlags(const QFlags<QScriptValue::PropertyFlag>& flags)
    {
        JSPropertyAttributes attr = 0;
        if (flags.testFlag(QScriptValue::ReadOnly))
            attr |= kJSPropertyAttributeReadOnly;
        if (flags.testFlag(QScriptValue::Undeletable))
            attr |= kJSPropertyAttributeDontDelete;
        if (flags.testFlag(QScriptValue::SkipInEnumeration))
            attr |= kJSPropertyAttributeDontEnum;
        return attr;
    }
};

#endif // qscriptconverter_p_h
