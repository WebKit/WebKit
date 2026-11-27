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

#include "SanitizerElementNamespaceWithAttributes.h"
#include "SanitizerProcessingInstruction.h"

namespace WebCore {

// https://html.spec.whatwg.org/multipage/dynamic-markup-insertion.html#sanitizerconfig
struct SanitizerConfig {
    std::optional<Vector<SanitizerElementWithAttributes>> elements;
    std::optional<Vector<SanitizerElement>> removeElements;
    std::optional<Vector<SanitizerElement>> replaceWithChildrenElements;

    std::optional<Vector<SanitizerPI>> processingInstructions;
    std::optional<Vector<SanitizerPI>> removeProcessingInstructions;

    std::optional<Vector<SanitizerAttribute>> attributes;
    std::optional<Vector<SanitizerAttribute>> removeAttributes;

    std::optional<bool> comments;
    std::optional<bool> dataAttributes;
    std::optional<bool> javascriptURLs;
};

} // namespace WebCore
