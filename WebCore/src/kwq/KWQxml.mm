/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <qstring.h>

#include <qxml.h>

// class QXmlAttributes ========================================================
    
QXmlAttributes::QXmlAttributes()
{
}


QXmlAttributes::QXmlAttributes(const QXmlAttributes &)
{
}


QXmlAttributes::~QXmlAttributes()
{
}


QString QXmlAttributes::value(const QString &) const
{
}


int QXmlAttributes::length() const
{
}


QString QXmlAttributes::localName(int index) const
{
}


QString QXmlAttributes::value(int index) const
{
}


QXmlAttributes &QXmlAttributes::operator=(const QXmlAttributes &)
{
}




// class QXmlInputSource ========================================================


QXmlInputSource::QXmlInputSource()
{
}


QXmlInputSource::~QXmlInputSource()
{
}


void QXmlInputSource::setData(const QString& data)
{
}




// class QXmlDTDHandler ========================================================


// class QXmlDeclHandler ========================================================


// class QXmlErrorHandler ========================================================


// class QXmlLexicalHandler ========================================================


// class QXmlContentHandler ========================================================


// class QXmlDefaultHandler ====================================================


QXmlDefaultHandler::~QXmlDefaultHandler()
{
}


// class QXmlSimpleReader ======================================================

QXmlSimpleReader::QXmlSimpleReader()
{
}


QXmlSimpleReader::~QXmlSimpleReader()
{
}


void QXmlSimpleReader::setContentHandler(QXmlContentHandler *handler)
{
}


bool QXmlSimpleReader::parse(const QXmlInputSource &input)
{
}


void QXmlSimpleReader::setLexicalHandler(QXmlLexicalHandler *handler)
{
}


void QXmlSimpleReader::setDTDHandler(QXmlDTDHandler *handler)
{
}


void QXmlSimpleReader::setDeclHandler(QXmlDeclHandler *handler)
{
}


void QXmlSimpleReader::setErrorHandler(QXmlErrorHandler *handler)
{
}


// class QXmlParseException ====================================================

QXmlParseException::QXmlParseException()
{
}


QString QXmlParseException::message() const
{
}


int QXmlParseException::columnNumber() const
{
}


int QXmlParseException::lineNumber() const
{
}




