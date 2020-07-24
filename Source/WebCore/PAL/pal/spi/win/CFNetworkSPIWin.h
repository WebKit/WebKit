/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <CFNetwork/CFNetwork.h>
#include <pal/spi/cf/CFNetworkConnectionCacheSPI.h>

#include <CFNetwork/CFHTTPCookiesPriv.h>
#include <CFNetwork/CFHTTPStream.h>
#include <CFNetwork/CFProxySupportPriv.h>
#include <CFNetwork/CFURLCachePriv.h>
#include <CFNetwork/CFURLConnectionPriv.h>
#include <CFNetwork/CFURLCredentialStorage.h>
#include <CFNetwork/CFURLProtocolPriv.h>
#include <CFNetwork/CFURLRequestPriv.h>
#include <CFNetwork/CFURLResponsePriv.h>
#include <CFNetwork/CFURLStorageSession.h>

WTF_EXTERN_C_BEGIN

CFN_EXPORT CFStringRef _CFNetworkErrorGetLocalizedDescription(CFIndex);

extern const CFStringRef _kCFWindowsSSLLocalCert;
extern const CFStringRef _kCFStreamPropertyWindowsSSLCertInfo;
extern const CFStringRef _kCFWindowsSSLPeerCert;

CFN_EXPORT const CFStringRef kCFStreamPropertyCONNECTProxy;
CFN_EXPORT const CFStringRef kCFStreamPropertyCONNECTProxyHost;
CFN_EXPORT const CFStringRef kCFStreamPropertyCONNECTProxyPort;
CFN_EXPORT const CFStringRef kCFStreamPropertyCONNECTAdditionalHeaders;
CFN_EXPORT const CFStringRef kCFStreamPropertyCONNECTResponse;
CFN_EXPORT void _CFHTTPMessageSetResponseURL(CFHTTPMessageRef, CFURLRef);
CFN_EXPORT void _CFHTTPMessageSetResponseProxyURL(CFHTTPMessageRef, CFURLRef);

WTF_EXTERN_C_END
