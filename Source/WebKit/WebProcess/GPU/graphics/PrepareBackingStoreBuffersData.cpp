/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
#include "PrepareBackingStoreBuffersData.h"

#include "BufferIdentifierSet.h"
#include "RemoteLayerBackingStore.h"
#include <wtf/text/TextStream.h>

namespace WebKit {

static TextStream& operator<<(TextStream& ts, const BufferIdentifierSet& identifierSet)
{
    ts << "front: " << identifierSet.front << " back: " << identifierSet.back << " secondaryBack: " << identifierSet.secondaryBack;
    return ts;
}

TextStream& operator<<(TextStream& ts, const PrepareBackingStoreBuffersInputData& inputData)
{
    ts << inputData.bufferSet << ", supportsPartialRepaint: " << inputData.supportsPartialRepaint << " hasEmptyDirtyRegion: " << inputData.hasEmptyDirtyRegion;
    return ts;
}

TextStream& operator<<(TextStream& ts, const PrepareBackingStoreBuffersOutputData& outputData)
{
    ts << outputData.bufferSet << ", has front buffer handle: " << !!outputData.frontBufferHandle << " displayRequirement: " << outputData.displayRequirement;
    return ts;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS) && PLATFORM(COCOA)
