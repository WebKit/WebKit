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

#include "config.h"
#include "WebCompiledContentExtensionData.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "ArgumentCoders.h"

namespace WebKit {

void WebCompiledContentExtensionData::encode(IPC::Encoder& encoder) const
{
    SharedMemory::Handle handle;
    data->createHandle(handle, SharedMemory::Protection::ReadOnly);
    encoder << handle;

    encoder << actionsOffset;
    encoder << actionsSize;
    encoder << filtersWithoutDomainsBytecodeOffset;
    encoder << filtersWithoutDomainsBytecodeSize;
    encoder << filtersWithDomainsBytecodeOffset;
    encoder << filtersWithDomainsBytecodeSize;
    encoder << domainFiltersBytecodeOffset;
    encoder << domainFiltersBytecodeSize;
}

bool WebCompiledContentExtensionData::decode(IPC::Decoder& decoder, WebCompiledContentExtensionData& compiledContentExtensionData)
{
    SharedMemory::Handle handle;
    if (!decoder.decode(handle))
        return false;
    compiledContentExtensionData.data = SharedMemory::map(handle, SharedMemory::Protection::ReadOnly);

    if (!decoder.decode(compiledContentExtensionData.actionsOffset))
        return false;
    if (!decoder.decode(compiledContentExtensionData.actionsSize))
        return false;
    if (!decoder.decode(compiledContentExtensionData.filtersWithoutDomainsBytecodeOffset))
        return false;
    if (!decoder.decode(compiledContentExtensionData.filtersWithoutDomainsBytecodeSize))
        return false;
    if (!decoder.decode(compiledContentExtensionData.filtersWithDomainsBytecodeOffset))
        return false;
    if (!decoder.decode(compiledContentExtensionData.filtersWithDomainsBytecodeSize))
        return false;
    if (!decoder.decode(compiledContentExtensionData.domainFiltersBytecodeOffset))
        return false;
    if (!decoder.decode(compiledContentExtensionData.domainFiltersBytecodeSize))
        return false;

    return true;
}

} // namespace WebKit

#endif // ENABLE(CONTENT_EXTENSIONS)
