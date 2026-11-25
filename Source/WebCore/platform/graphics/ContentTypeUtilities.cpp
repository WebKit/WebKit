/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ContentTypeUtilities.h"

#include "FourCC.h"
#include <algorithm>

namespace WebCore {

bool contentTypeMeetsContainerAndCodecTypeRequirements(const ContentType& type, const std::optional<Vector<RefPtr<StringImpl>>>& allowedMediaContainerTypes, const std::optional<Vector<RefPtr<StringImpl>>>& allowedMediaCodecTypes)
{
    if (allowedMediaContainerTypes && !allowedMediaContainerTypes->containsIf([&](auto& item) { return equal(item, type.containerType()); }))
        return false;

    if (!allowedMediaCodecTypes)
        return true;

    return std::ranges::all_of(type.codecs(), [&](auto& codec) {
        return std::ranges::any_of(*allowedMediaCodecTypes, [&](auto& allowedCodec) {
            return startsWith(codec, allowedCodec);
        });
    });
}

}
