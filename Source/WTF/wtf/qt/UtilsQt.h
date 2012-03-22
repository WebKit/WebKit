/*
    Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef WTF_UtilsQt_h
#define WTF_UtilsQt_h

#include <QString>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QTextDocument>
#endif

inline QString escapeHtml(const QString& string)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    return string.toHtmlEscaped();
#else
    return Qt::escape(string);
#endif
}

#endif // WTF_UtilsQt_h
