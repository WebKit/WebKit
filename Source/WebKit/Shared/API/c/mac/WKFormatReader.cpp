/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WKFormatReader.h"

#include "FormatReader.h"

#include <pal/cocoa/MediaToolboxSoftLink.h>

OSStatus WKFormatReaderCreate(CFAllocatorRef allocator, MTPluginFormatReaderRef* formatReaderRef)
{
#if ENABLE(WEBM_FORMAT_READER)
    auto formatReader = WebKit::FormatReader::create(allocator);
    if (!formatReader)
        return kMTPluginFormatReaderError_AllocationFailure;
    *formatReaderRef = formatReader.leakRef()->wrapper();
    return noErr;
#else
    UNUSED_PARAM(allocator);
    *formatReaderRef = nullptr;
    return -1;
#endif
}

OSStatus WKFormatReaderStartOnMainThread(MTPluginFormatReaderRef formatReaderRef, MTPluginByteSourceRef byteSource)
{
#if ENABLE(WEBM_FORMAT_READER)
    auto formatReader = WebKit::FormatReader::unwrap(formatReaderRef);
    if (!formatReader)
        return paramErr;
    formatReader->startOnMainThread(byteSource);
    return noErr;
#else
    UNUSED_PARAM(formatReaderRef);
    UNUSED_PARAM(byteSource);
    return -1;
#endif
}
