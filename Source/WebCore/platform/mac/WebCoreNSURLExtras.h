/*
 * Copyright (C) 2005, 2007, 2012, 2014 Apple, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

@class NSString;
@class NSURL;

namespace WebCore {

WEBCORE_EXPORT NSString *userVisibleString(NSURL *);
WEBCORE_EXPORT NSURL *URLByCanonicalizingURL(NSURL *);
WEBCORE_EXPORT NSURL *URLWithUserTypedString(NSString *, NSURL *baseURL); // Return value of nil means error.
WEBCORE_EXPORT NSURL *URLByRemovingUserInfo(NSURL *);
WEBCORE_EXPORT BOOL hostNameNeedsDecodingWithRange(NSString *, NSRange, BOOL* error);
WEBCORE_EXPORT BOOL hostNameNeedsEncodingWithRange(NSString *, NSRange, BOOL* error);
WEBCORE_EXPORT NSString *decodeHostNameWithRange(NSString *, NSRange); // Return value of nil means error.
WEBCORE_EXPORT NSString *encodeHostNameWithRange(NSString *, NSRange); // Return value of nil means error.
WEBCORE_EXPORT NSString *decodeHostName(NSString *); // Return value of nil means error.
WEBCORE_EXPORT NSString *encodeHostName(NSString *); // Return value of nil means error.
WEBCORE_EXPORT NSURL *URLByTruncatingOneCharacterBeforeComponent(NSURL *, CFURLComponentType);
WEBCORE_EXPORT NSURL *URLWithData(NSData *, NSURL *baseURL);
WEBCORE_EXPORT NSData *originalURLData(NSURL *);
WEBCORE_EXPORT NSData *dataForURLComponentType(NSURL *, CFURLComponentType);
WEBCORE_EXPORT NSURL *URLWithUserTypedStringDeprecated(NSString *, NSURL *baseURL);

NSRange rangeOfURLScheme(NSString *);
WEBCORE_EXPORT BOOL isUserVisibleURL(NSString *);
WEBCORE_EXPORT BOOL looksLikeAbsoluteURL(NSString *);

} // namespace WebCore
