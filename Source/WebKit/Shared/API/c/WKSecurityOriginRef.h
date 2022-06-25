/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WKSecurityOriginRef_h
#define WKSecurityOriginRef_h

#include <WebKit/WKBase.h>

#ifdef __cplusplus
extern "C" {
#endif

WK_EXPORT WKTypeID WKSecurityOriginGetTypeID(void);

WK_EXPORT WKSecurityOriginRef WKSecurityOriginCreateFromString(WKStringRef string);
WK_EXPORT WKSecurityOriginRef WKSecurityOriginCreateFromDatabaseIdentifier(WKStringRef identifier);
WK_EXPORT WKSecurityOriginRef WKSecurityOriginCreate(WKStringRef protocol, WKStringRef host, int port);

WK_EXPORT WKStringRef WKSecurityOriginCopyDatabaseIdentifier(WKSecurityOriginRef securityOrigin);
WK_EXPORT WKStringRef WKSecurityOriginCopyToString(WKSecurityOriginRef securityOrigin);
WK_EXPORT WKStringRef WKSecurityOriginCopyProtocol(WKSecurityOriginRef securityOrigin);
WK_EXPORT WKStringRef WKSecurityOriginCopyHost(WKSecurityOriginRef securityOrigin);
WK_EXPORT unsigned short WKSecurityOriginGetPort(WKSecurityOriginRef securityOrigin);

#ifdef __cplusplus
}
#endif

#endif /* WKSecurityOriginRef_h */
