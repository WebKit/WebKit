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

#include "config.h"
#import "KWQKLocale.h"

#import "KWQExceptions.h"
#import "KWQLogging.h"
#import "KWQString.h"
#import "WebCoreViewFactory.h"

QString inputElementAltText()
{
    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([[WebCoreViewFactory sharedFactory] inputElementAltText]);
    KWQ_UNBLOCK_EXCEPTIONS;

    return QString();
}

QString resetButtonDefaultLabel()
{
    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([[WebCoreViewFactory sharedFactory] resetButtonDefaultLabel]);
    KWQ_UNBLOCK_EXCEPTIONS;

    return QString();
}

QString searchableIndexIntroduction()
{
    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([[WebCoreViewFactory sharedFactory] searchableIndexIntroduction]);
    KWQ_UNBLOCK_EXCEPTIONS;

    return QString();
}

QString submitButtonDefaultLabel()
{
    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([[WebCoreViewFactory sharedFactory] submitButtonDefaultLabel]);
    KWQ_UNBLOCK_EXCEPTIONS;

    return QString();
}

QString KLocale::language()
{
    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([[WebCoreViewFactory sharedFactory] defaultLanguageCode]);
    KWQ_UNBLOCK_EXCEPTIONS;

    return QString();
}
