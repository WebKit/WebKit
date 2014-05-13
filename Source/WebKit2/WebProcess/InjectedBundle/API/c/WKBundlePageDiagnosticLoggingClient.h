/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef WKBundlePageDiagnosticLoggingClient_h
#define WKBundlePageDiagnosticLoggingClient_h

#include <WebKit/WKBase.h>

typedef void (*WKBundlePageDiagnosticLoggingCallback)(WKBundlePageRef page, WKStringRef message, WKStringRef description, WKStringRef success, const void* clientInfo);

typedef struct WKBundlePageDiagnosticLoggingClientBase {
    int                                                                 version;
    const void *                                                        clientInfo;
} WKBundlePageDiagnosticLoggingClientBase;

typedef struct WKBundlePageDiagnosticLoggingClientV0 {
    WKBundlePageDiagnosticLoggingClientBase                             base;

    // Version 0.
    WKBundlePageDiagnosticLoggingCallback                               logDiagnosticMessage;
} WKBundlePageDiagnosticLoggingClientV0;

enum { kWKBundlePageDiagnosticLoggingClientCurrentVersion WK_ENUM_DEPRECATED("Use an explicit version number instead") = 0 };
typedef struct WKBundlePageDiagnosticLoggingClient {
    int                                                                 version;
    const void *                                                        clientInfo;

    // Version 0.
    WKBundlePageDiagnosticLoggingCallback                               logDiagnosticMessage;
} WKBundlePageDiagnosticLoggingClient WK_DEPRECATED("Use an explicit versioned struct instead");

#endif // WKBundlePageDiagnosticLoggingClient_h
