/*
 * Copyright (C) 2004 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include <wtf/EnumTraits.h>

namespace WebCore {

// FIXME: This really should be broken up into more than one concept.
// Frame doesn't need the 3 boundaries in this enum.
enum class TextGranularity : uint8_t {
    CharacterGranularity,
    WordGranularity,
    SentenceGranularity,
    LineGranularity,
    ParagraphGranularity,
    DocumentGranularity,
    SentenceBoundary,
    LineBoundary,
    ParagraphBoundary,
    DocumentBoundary
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::TextGranularity> {
    using values = EnumValues<
        WebCore::TextGranularity,
        WebCore::TextGranularity::CharacterGranularity,
        WebCore::TextGranularity::WordGranularity,
        WebCore::TextGranularity::SentenceGranularity,
        WebCore::TextGranularity::LineGranularity,
        WebCore::TextGranularity::ParagraphGranularity,
        WebCore::TextGranularity::DocumentGranularity,
        WebCore::TextGranularity::SentenceBoundary,
        WebCore::TextGranularity::LineBoundary,
        WebCore::TextGranularity::ParagraphBoundary,
        WebCore::TextGranularity::DocumentBoundary
    >;
};

}
