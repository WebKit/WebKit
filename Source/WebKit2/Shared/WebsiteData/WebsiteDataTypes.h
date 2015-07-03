/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef WebsiteDataTypes_h
#define WebsiteDataTypes_h

namespace WebKit {

enum WebsiteDataTypes {
    WebsiteDataTypeCookies = 1 << 0,
    WebsiteDataTypeDiskCache = 1 << 1,
    WebsiteDataTypeMemoryCache = 1 << 2,
    WebsiteDataTypeOfflineWebApplicationCache = 1 << 3,
    WebsiteDataTypeSessionStorage = 1 << 4,
    WebsiteDataTypeLocalStorage = 1 << 5,
    WebsiteDataTypeWebSQLDatabases = 1 << 6,
    WebsiteDataTypeIndexedDBDatabases = 1 << 7,
    WebsiteDataTypeMediaKeys = 1 << 8,
    WebsiteDataTypeHSTSCache = 1 << 9,
#if ENABLE(NETSCAPE_PLUGIN_API)
    WebsiteDataTypePlugInData = 1 << 10,
#endif
};

};

#endif // WebsiteDataTypes_h
