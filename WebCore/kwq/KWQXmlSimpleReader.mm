/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#include "KWQXmlSimpleReader.h"

#include "expat.h"

#include "KWQAssertions.h"
#include "KWQXmlAttributes.h"

static void startElementHandler(void *userData, const char *name, const char **expatAttributes)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    QString nameString = QString::fromUtf8(name);
    QXmlAttributes attributes(expatAttributes);
    reader->contentHandler()->startElement(QString::null, nameString, nameString, attributes);
}

static void endElementHandler(void *userData, const char *name)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    QString nameString = QString::fromUtf8(name);
    reader->contentHandler()->endElement(QString::null, nameString, nameString);
}

static void defaultHandler(void *userData, const char *s, int len)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    reader->contentHandler()->characters(QString::fromUtf8(s, len));
}

static void processingInstructionHandler(void *userData, const char *target, const char *data)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    reader->contentHandler()->processingInstruction(QString::fromUtf8(target), QString::fromUtf8(data));
}

static void characterDataHandler(void *userData, const char *s, int len)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    reader->contentHandler()->characters(QString::fromUtf8(s, len));
}

static void startCdataSectionHandler(void *userData)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    reader->lexicalHandler()->startCDATA();
}

static void endCdataSectionHandler(void *userData)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    reader->lexicalHandler()->endCDATA();
}

static void commentHandler(void *userData, const char *comment)
{
    QXmlSimpleReader *reader = static_cast<QXmlSimpleReader *>(userData);
    reader->lexicalHandler()->comment(QString::fromUtf8(comment));
}

QXmlSimpleReader::QXmlSimpleReader()
    : _contentHandler(0)
    , _declarationHandler(0)
    , _DTDHandler(0)
    , _errorHandler(0)
    , _lexicalHandler(0)
{
}

bool QXmlSimpleReader::parse(const QXmlInputSource &input)
{
    if (_contentHandler && !_contentHandler->startDocument()) {
        return false;
    }
    const QChar BOM(0xFEFF);
    const unsigned char BOMHighByte = *reinterpret_cast<const unsigned char *>(&BOM);
    XML_Parser parser = XML_ParserCreate(BOMHighByte == 0xFF ? "UTF-16LE" : "UTF-16BE");
    XML_SetUserData(parser, this);
    if (_contentHandler) {
        XML_SetCharacterDataHandler(parser, characterDataHandler);
        XML_SetDefaultHandler(parser, defaultHandler);
        XML_SetEndElementHandler(parser, endElementHandler);
        XML_SetProcessingInstructionHandler(parser, processingInstructionHandler);
        XML_SetStartElementHandler(parser, startElementHandler);
    }
    if (_lexicalHandler) {
        XML_SetEndCdataSectionHandler(parser, endCdataSectionHandler);
        XML_SetStartCdataSectionHandler(parser, startCdataSectionHandler);
        XML_SetCommentHandler(parser, commentHandler);
    }
    XML_Status parseError = XML_Parse(parser,
        reinterpret_cast<const char *>(input.data().unicode()),
        input.data().length() * sizeof(QChar), TRUE);
    XML_ParserFree(parser);
    if (_contentHandler && !_contentHandler->endDocument()) {
        return false;
    }
    return parseError != XML_STATUS_ERROR;
}

QString QXmlParseException::message() const
{
    ERROR("not yet implemented");
    return QString::null;
}

int QXmlParseException::columnNumber() const
{
    ERROR("not yet implemented");
    return 0;
}

int QXmlParseException::lineNumber() const
{
    ERROR("not yet implemented");
    return 0;
}
