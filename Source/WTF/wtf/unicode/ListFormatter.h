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

#include <memory>
#include <optional>
#include <span>
#include <wtf/ExportMacros.h>
#include <wtf/Noncopyable.h>
#include <wtf/Range.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

struct UListFormatter;

namespace WTF {

class ListFormatter {
    WTF_MAKE_NONCOPYABLE(ListFormatter);
public:
    enum class Type : uint8_t { Conjunction, Disjunction, Unit };
    enum class Style : uint8_t { Long, Short, Narrow };

    ListFormatter() = default;
    // The locale is a BCP 47 language tag. Check the result with operator bool.
    WTF_EXPORT_PRIVATE ListFormatter(const ASCIICString& locale, Type, Style);
    ListFormatter(ListFormatter&&) = default;
    ListFormatter& operator=(ListFormatter&&) = default;

    explicit operator bool() const { return !!m_formatter; }

    WTF_EXPORT_PRIVATE std::optional<String> format(std::span<const String>) const;
    WTF_EXPORT_PRIVATE std::optional<String> format(std::span<const std::span<const char16_t>>) const;

    struct FormattedList {
        String string;
        Vector<Range<unsigned>> elementRanges;
    };
    WTF_EXPORT_PRIVATE std::optional<FormattedList> formatToParts(std::span<const String>) const;

private:
    struct Deleter {
        WTF_EXPORT_PRIVATE void operator()(UListFormatter*) const;
    };
    std::unique_ptr<UListFormatter, Deleter> m_formatter;
};

} // namespace WTF

using WTF::ListFormatter;
