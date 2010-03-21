/*
    Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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

#ifndef qscriptprogram_h
#define qscriptprogram_h

#include "qtscriptglobal.h"
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>

class QScriptProgramPrivate;
class Q_JAVASCRIPT_EXPORT QScriptProgram {
public:
    QScriptProgram();
    QScriptProgram(const QString& sourceCode,
                   const QString fileName = QString(),
                   int firstLineNumber = 1);
    QScriptProgram(const QScriptProgram& other);
    ~QScriptProgram();

    QScriptProgram& operator=(const QScriptProgram& other);

    bool isNull() const;

    QString sourceCode() const;
    QString fileName() const;
    int firstLineNumber() const;

    bool operator==(const QScriptProgram& other) const;
    bool operator!=(const QScriptProgram& other) const;

private:
    QExplicitlySharedDataPointer<QScriptProgramPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QScriptProgram)
};

#endif // qscriptprogram_h
