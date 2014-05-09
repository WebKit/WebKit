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

#ifndef WKBundlePageContextMenuClient_h
#define WKBundlePageContextMenuClient_h

#include <WebKit/WKBase.h>

typedef void (*WKBundlePageGetContextMenuFromDefaultContextMenuCallback)(WKBundlePageRef page, WKBundleHitTestResultRef hitTestResult, WKArrayRef defaultMenu, WKArrayRef* newMenu, WKTypeRef* userData, const void* clientInfo);

typedef struct WKBundlePageContextMenuClientBase {
    int                                                                 version;
    const void *                                                        clientInfo;
} WKBundlePageContextMenuClientBase;

typedef struct WKBundlePageContextMenuClientV0 {
    WKBundlePageContextMenuClientBase                                   base;

    // Version 0.
    WKBundlePageGetContextMenuFromDefaultContextMenuCallback            getContextMenuFromDefaultMenu;
} WKBundlePageContextMenuClientV0;

enum { kWKBundlePageContextMenuClientCurrentVersion WK_ENUM_DEPRECATED("Use an explicit version number instead") = 0 };
typedef struct WKBundlePageContextMenuClient {
    int                                                                 version;
    const void *                                                        clientInfo;

    // Version 0.
    WKBundlePageGetContextMenuFromDefaultContextMenuCallback            getContextMenuFromDefaultMenu;
} WKBundlePageContextMenuClient WK_DEPRECATED("Use an explicit versioned struct instead");

#endif // WKBundlePageContextMenuClient_h
