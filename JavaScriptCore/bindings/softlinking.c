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

#include "softlinking.h"

#include <mach-o/dyld.h>

static const struct mach_header *loadFramework(const char *execPath)
{
    return NSAddImage(execPath, NSADDIMAGE_OPTION_WITH_SEARCHING | NSADDIMAGE_OPTION_MATCH_FILENAME_BY_INSTALLNAME);
}

static void *getFunctionPointer(const struct mach_header *header, const char *functionName)
{
    NSSymbol symbol = NSLookupSymbolInImage(header, functionName, NSLOOKUPSYMBOLINIMAGE_OPTION_BIND);
    if (symbol!=NULL) {
        return NSAddressOfSymbol(symbol);
    }
    return 0;
}

jint KJS_GetCreatedJavaVMs(JavaVM **vmBuf, jsize bufLen, jsize *nVMs)
{
    static const struct mach_header *header = 0;
    if (!header) {
        header = loadFramework("/System/Library/Frameworks/JavaVM.framework/JavaVM");
    }
    static jint(*functionPointer)(JavaVM **, jsize, jsize *) = 0;
    if (!functionPointer) {
        functionPointer = getFunctionPointer(header, "_JNI_GetCreatedJavaVMs");
    }
    return functionPointer(vmBuf, bufLen, nVMs);
}
