/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef WKPageDiagnosticLoggingClient_h
#define WKPageDiagnosticLoggingClient_h

#include <WebKit/WKBase.h>
#include <WebKit/WKDiagnosticLoggingResultType.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*WKPageLogDiagnosticMessageCallback)(WKPageRef page, WKStringRef message, WKStringRef description, const void* clientInfo);
typedef void (*WKPageLogDiagnosticMessageWithResultCallback)(WKPageRef page, WKStringRef message, WKStringRef description, WKDiagnosticLoggingResultType result, const void* clientInfo);
typedef void (*WKPageLogDiagnosticMessageWithValueCallback)(WKPageRef page, WKStringRef message, WKStringRef description, WKStringRef value, const void* clientInfo);
typedef void (*WKPageLogDiagnosticMessageWithEnhancedPrivacyCallback)(WKPageRef page, WKStringRef message, WKStringRef description, const void* clientInfo);

typedef struct WKPageDiagnosticLoggingClientBase {
    int                                                                version;
    const void *                                                       clientInfo;
} WKPageDiagnosticLoggingClientBase;

typedef struct WKPageDiagnosticLoggingClientV0 {
    WKPageDiagnosticLoggingClientBase                                  base;

    // Version 0.
    WKPageLogDiagnosticMessageCallback                                 logDiagnosticMessage;
    WKPageLogDiagnosticMessageWithResultCallback                       logDiagnosticMessageWithResult;
    WKPageLogDiagnosticMessageWithValueCallback                        logDiagnosticMessageWithValue;
} WKPageDiagnosticLoggingClientV0;

typedef struct WKPageDiagnosticLoggingClientV1 {
    WKPageDiagnosticLoggingClientBase                                  base;

    // Version 0.
    WKPageLogDiagnosticMessageCallback                                 logDiagnosticMessage;
    WKPageLogDiagnosticMessageWithResultCallback                       logDiagnosticMessageWithResult;
    WKPageLogDiagnosticMessageWithValueCallback                        logDiagnosticMessageWithValue;

    // Version 1.
    WKPageLogDiagnosticMessageWithEnhancedPrivacyCallback              logDiagnosticMessageWithEnhancedPrivacy;
} WKPageDiagnosticLoggingClientV1;

#ifdef __cplusplus
}
#endif

#endif // WKPageDiagnosticLoggingClient_h
