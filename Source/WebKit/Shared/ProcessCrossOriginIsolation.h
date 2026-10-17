/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <WebCore/CrossOriginMode.h>

namespace WebKit {

enum class ProcessCrossOriginIsolation : uint8_t {
    None,
    // For a browsing context group that is cross-origin isolated.
    BrowsingContextGroup,
    // For documents with a Document-Isolation-Policy in a browsing context group that is not.
    DocumentIsolationPolicy,
};

constexpr ProcessCrossOriginIsolation processCrossOriginIsolationForBrowsingContextGroup(WebCore::CrossOriginMode crossOriginMode)
{
    return crossOriginMode == WebCore::CrossOriginMode::Isolated ? ProcessCrossOriginIsolation::BrowsingContextGroup : ProcessCrossOriginIsolation::None;
}

constexpr WebCore::CrossOriginMode toCrossOriginMode(ProcessCrossOriginIsolation crossOriginIsolation)
{
    return crossOriginIsolation == ProcessCrossOriginIsolation::None ? WebCore::CrossOriginMode::Shared : WebCore::CrossOriginMode::Isolated;
}

} // namespace WebKit
