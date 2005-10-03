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
#import "KWQKCookieJar.h"

#import "KWQExceptions.h"
#import "KWQKURL.h"
#import "WebCoreCookieAdapter.h"
#import <Foundation/NSString.h>

QString KWQKCookieJar::cookie(const KURL &url)
{
    KWQ_BLOCK_EXCEPTIONS;
    return QString::fromNSString([[WebCoreCookieAdapter sharedAdapter] cookiesForURL:url.url().getNSString()]);
    KWQ_UNBLOCK_EXCEPTIONS;

    return QString();
}

void KWQKCookieJar::setCookie(const KURL &url, const KURL &policyBaseURL, const QString &cookie)
{
    KWQ_BLOCK_EXCEPTIONS;

    [[WebCoreCookieAdapter sharedAdapter] setCookies:cookie.getNSString()
     forURL:url.url().getNSString() policyBaseURL:policyBaseURL.url().getNSString()];

    KWQ_UNBLOCK_EXCEPTIONS;
}

bool KWQKCookieJar::cookieEnabled()
{
    KWQ_BLOCK_EXCEPTIONS;
    return [[WebCoreCookieAdapter sharedAdapter] cookiesEnabled];
    KWQ_UNBLOCK_EXCEPTIONS;

    return false;
}
