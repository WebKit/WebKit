/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "PlatformMediaCapabilitiesTransferFunction.h"

namespace WebCore {

enum class TransferFunction : uint8_t {
    SRGB,
    PQ,
    HLG
};

inline PlatformMediaCapabilitiesTransferFunction toPlatform(TransferFunction value)
{
    switch (value) {
    case TransferFunction::SRGB:    return PlatformMediaCapabilitiesTransferFunction::SRGB;
    case TransferFunction::PQ:      return PlatformMediaCapabilitiesTransferFunction::PQ;
    case TransferFunction::HLG:     return PlatformMediaCapabilitiesTransferFunction::HLG;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline TransferFunction fromPlatform(PlatformMediaCapabilitiesTransferFunction value)
{
    switch (value) {
    case PlatformMediaCapabilitiesTransferFunction::SRGB:    return TransferFunction::SRGB;
    case PlatformMediaCapabilitiesTransferFunction::PQ:      return TransferFunction::PQ;
    case PlatformMediaCapabilitiesTransferFunction::HLG:     return TransferFunction::HLG;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebCore
