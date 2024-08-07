/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#pragma once

#include "GPUBindGroupEntry.h"
#include "GPUBindGroupLayout.h"
#include "GPUObjectDescriptorBase.h"
#include "WebGPUBindGroupDescriptor.h"
#include <wtf/Vector.h>

namespace WebCore {

struct GPUBindGroupDescriptor : public GPUObjectDescriptorBase {
    WebGPU::BindGroupDescriptor convertToBacking() const
    {
        ASSERT(layout);
        return {
            { label },
            layout->backing(),
            entries.map([](auto& bindGroupEntry) {
                return bindGroupEntry.convertToBacking();
            }),
        };
    }

    const RefPtr<GPUExternalTexture>* externalTextureMatches(Vector<GPUBindGroupEntry>& comparisonEntries, bool& hasExternalTexture) const
    {
        bool matched = true;
        hasExternalTexture = false;
        auto entriesSize = entries.size();
        if (entriesSize != comparisonEntries.size())
            matched = false;

        const RefPtr<GPUExternalTexture>* result = nullptr;
        for (size_t i = 0; i < entriesSize; ++i) {
            auto& entry = entries[i];
            if (matched && !GPUBindGroupEntry::equal(entry, comparisonEntries[i]))
                matched = false;

            auto externalTexture = entry.externalTexture();
            if (!result)
                result = externalTexture;
            else if (externalTexture)
                return nullptr;
            if (result)
                hasExternalTexture = true;
        }

        return matched ? result : nullptr;
    }

    WeakPtr<GPUBindGroupLayout> layout;
    Vector<GPUBindGroupEntry> entries;
};

}
