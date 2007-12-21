/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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

#ifndef ForEachCoClass_h
#define ForEachCoClass_h

#include "ProgIDMacros.h"

#define FOR_EACH_COCLASS(macro) \
    macro(CFDictionaryPropertyBag) \
    macro(WebCache) \
    macro(WebDatabaseManager) \
    macro(WebDownload) \
    macro(WebError) \
    macro(WebHistory) \
    macro(WebHistoryItem) \
    macro(WebIconDatabase) \
    macro(WebJavaScriptCollector) \
    macro(WebKitStatistics) \
    macro(WebMutableURLRequest) \
    macro(WebNotificationCenter) \
    macro(WebPreferences) \
    macro(WebScrollBar) \
    macro(WebScriptDebugServer) \
    macro(WebTextRenderer) \
    macro(WebURLCredential) \
    macro(WebURLProtectionSpace) \
    macro(WebURLRequest) \
    macro(WebURLResponse) \
    macro(WebView) \
    // end of macro

#define WEBKITCLASS_MEMBER(cls) cls##Class,
enum WebKitClass {
    FOR_EACH_COCLASS(WEBKITCLASS_MEMBER)
    WebKitClassSentinel
};
#undef WEBKITCLASS_MEMBER

#define PRODUCTION_PROGID(cls) VERSION_INDEPENDENT_PRODUCTION_PROGID(cls),
static LPCOLESTR productionProgIDs[WebKitClassSentinel] = {
    FOR_EACH_COCLASS(PRODUCTION_PROGID)
};
#undef PRODUCTION_PROGID

#define OPENSOURCE_PROGID(cls) VERSION_INDEPENDENT_OPENSOURCE_PROGID(cls),
static LPCOLESTR openSourceProgIDs[WebKitClassSentinel] = {
    FOR_EACH_COCLASS(OPENSOURCE_PROGID)
};
#undef OPENSOURCE_PROGID

#if __PRODUCTION__
    static LPCOLESTR* s_progIDs = productionProgIDs;
#else
    static LPCOLESTR* s_progIDs = openSourceProgIDs;
#endif

#define PROGID(className) progIDForClass(className##Class)

void setUseOpenSourceWebKit(bool);
LPCOLESTR progIDForClass(WebKitClass);



#endif // !defined(ForEachCoClass_h)
