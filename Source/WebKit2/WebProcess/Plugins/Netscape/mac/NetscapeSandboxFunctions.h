/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef NetscapeSandboxFunctions_h
#define NetscapeSandboxFunctions_h

#if ENABLE(NETSCAPE_PLUGIN_API)

#include <WebCore/npapi.h>

#ifdef __cplusplus
extern "C" {
#endif
    
#define WKNVSandboxFunctions 74659
#define WKNVSandboxFunctionsVersionCurrent 1

typedef NPError (*WKN_EnterSandboxProcPtr)(const char *readOnlyPaths[], const char *readWritePaths[]);
typedef NPError (*WKN_FileStopAccessingProcPtr)(const char* path);

NPError WKN_EnterSandbox(const char *readOnlyPaths[], const char *readWritePaths[]);
NPError WKN_FileStopAccessing(const char* path);

typedef struct _WKNSandboxFunctions {
    uint16_t size;
    uint16_t version;
    
    WKN_EnterSandboxProcPtr enterSandbox;
    WKN_FileStopAccessingProcPtr fileStopAccessing;
} WKNSandboxFunctions;

WKNSandboxFunctions* netscapeSandboxFunctions();

#ifdef __cplusplus
}
#endif

#endif // ENABLE(NETSCAPE_PLUGIN_API)

#endif
