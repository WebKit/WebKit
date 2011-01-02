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

#include "config.h"

#include "qscriptprogram.h"

#include "qscriptprogram_p.h"

/*!
  \internal

  \class QScriptProgram

  \brief The QScriptProgram class encapsulates a Qt Script program.

  \ingroup script

  QScriptProgram retains the compiled representation of the script if
  possible. Thus, QScriptProgram can be used to evaluate the same
  script multiple times more efficiently.

  \code
  QScriptEngine engine;
  QScriptProgram program("1 + 2");
  QScriptValue result = engine.evaluate(program);
  \endcode
*/

/*!
  Constructs a null QScriptProgram.
*/
QScriptProgram::QScriptProgram()
    : d_ptr(new QScriptProgramPrivate)
{}

/*!
  Constructs a new QScriptProgram with the given \a sourceCode, \a
  fileName and \a firstLineNumber.
*/
QScriptProgram::QScriptProgram(const QString& sourceCode,
               const QString fileName,
               int firstLineNumber)
    : d_ptr(new QScriptProgramPrivate(sourceCode, fileName, firstLineNumber))
{}

/*!
  Destroys this QScriptProgram.
*/
QScriptProgram::~QScriptProgram()
{}

/*!
  Constructs a new QScriptProgram that is a copy of \a other.
*/
QScriptProgram::QScriptProgram(const QScriptProgram& other)
{
    d_ptr = other.d_ptr;
}

/*!
  Assigns the \a other value to this QScriptProgram.
*/
QScriptProgram& QScriptProgram::operator=(const QScriptProgram& other)
{
    d_ptr = other.d_ptr;
    return *this;
}

/*!
  Returns true if this QScriptProgram is null; otherwise
  returns false.
*/
bool QScriptProgram::isNull() const
{
    return d_ptr->isNull();
}

/*!
  Returns the source code of this program.
*/
QString QScriptProgram::sourceCode() const
{
    return d_ptr->sourceCode();
}

/*!
  Returns the filename associated with this program.
*/
QString QScriptProgram::fileName() const
{
    return d_ptr->fileName();
}

/*!
  Returns the line number associated with this program.
*/
int QScriptProgram::firstLineNumber() const
{
    return d_ptr->firstLineNumber();
}

/*!
  Returns true if this QScriptProgram is equal to \a other;
  otherwise returns false.
*/
bool QScriptProgram::operator==(const QScriptProgram& other) const
{
    return d_ptr == other.d_ptr || *d_ptr == *other.d_ptr;
}

/*!
  Returns true if this QScriptProgram is not equal to \a other;
  otherwise returns false.
*/
bool QScriptProgram::operator!=(const QScriptProgram& other) const
{
    return d_ptr != other.d_ptr && *d_ptr != *other.d_ptr;
}

