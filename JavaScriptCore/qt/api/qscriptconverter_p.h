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
#include <QtCore/qstring.h>

/*
  \internal
  \class QScriptConverter
  QScriptValue and QScriptEngine helper class. This class's responsibility is to convert values
  between JS values and Qt/C++ values.

  This is a nice way to inline these functions in both QScriptValue and QScriptEngine.
*/
class QScriptConverter {
public:
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
};

#endif // qscriptconverter_p_h
