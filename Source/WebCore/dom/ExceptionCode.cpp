/*
 *  Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
#include "ExceptionCode.h"

#include "DOMCoreException.h"
#include "EventException.h"
#if ENABLE(BLOB)
#include "FileException.h"
#endif
#if ENABLE(INDEXED_DATABASE)
#include "IDBDatabaseException.h"
#endif
#if ENABLE(BLOB)
#include "OperationNotAllowedException.h"
#endif
#include "RangeException.h"
#if ENABLE(SQL_DATABASE)
#include "SQLException.h"
#endif
#if ENABLE(SVG)
#include "SVGException.h"
#endif
#include "XMLHttpRequestException.h"
#include "XPathException.h"

namespace WebCore {

void getExceptionCodeDescription(ExceptionCode ec, ExceptionCodeDescription& description)
{
    ASSERT(ec);

    if (EventException::initializeDescription(ec, &description))
        return;
#if ENABLE(BLOB)
    if (FileException::initializeDescription(ec, &description))
        return;
#endif
#if ENABLE(INDEXED_DATABASE)
    if (IDBDatabaseException::initializeDescription(ec, &description))
        return;
#endif
#if ENABLE(BLOB)
    if (OperationNotAllowedException::initializeDescription(ec, &description))
        return;
#endif
    if (RangeException::initializeDescription(ec, &description))
        return;
#if ENABLE(SQL_DATABASE)
    if (SQLException::initializeDescription(ec, &description))
        return;
#endif
#if ENABLE(SVG)
    if (SVGException::initializeDescription(ec, &description))
        return;
#endif
    if (XMLHttpRequestException::initializeDescription(ec, &description))
        return;
    if (XPathException::initializeDescription(ec, &description))
        return;
    if (DOMCoreException::initializeDescription(ec, &description))
        return;

    ASSERT_NOT_REACHED();
}

} // namespace WebCore
