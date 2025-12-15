/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <array>
#include <span>
#include <unicode/ucsdet.h>
#include <unicode/utf16.h>
#include <unicode/utf8.h>

namespace WTF {

WTF_EXPORT_PRIVATE std::span<const UCharsetMatch*> ucsdet_detectAll_span(UCharsetDetector*, UErrorCode* status);

// This is like U8_NEXT().
WTF_EXPORT_PRIVATE char32_t u8Next(std::span<const char8_t> string, size_t& offset);
// This is like U8_NEXT_OR_FFFD().
WTF_EXPORT_PRIVATE char32_t u8NextOrFFFD(std::span<const char8_t> string, size_t& offset);
// This is like U16_GET(). Use a subspan if you need to specify a start offset.
WTF_EXPORT_PRIVATE char32_t u16Get(std::span<const char16_t> string, size_t offset);
// This is like U16_NEXT().
WTF_EXPORT_PRIVATE char32_t u16Next(std::span<const char16_t> string, size_t& offset);
// This is like U16_NEXT_OR_FFFD().
WTF_EXPORT_PRIVATE char32_t u16NextOrFFFD(std::span<const char16_t> string, size_t& offset);
// This is like U16_PREV().
WTF_EXPORT_PRIVATE char32_t u16Prev(std::span<const char16_t> string, size_t& offset);

} // namespace WTF

using WTF::ucsdet_detectAll_span;
using WTF::u8Next;
using WTF::u8NextOrFFFD;
using WTF::u16Get;
using WTF::u16Next;
using WTF::u16NextOrFFFD;
using WTF::u16Prev;
