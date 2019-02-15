/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Optional.h>
#include <wtf/text/WTFString.h>

namespace WTF {
namespace EnvironmentImpl {

WTF_EXPORT_PRIVATE Optional<String> get(const String&);

inline const char* getRaw(const String& variable)
{
    auto string = get(variable);
    return string ? string->utf8().data() : nullptr;
}

inline Optional<int> getInt(const String& variable)
{
    auto string = get(variable);
    if (!string)
        return WTF::nullopt;

    bool ok;
    auto value = string->toIntStrict(&ok);
    return ok ? makeOptional(value) : WTF::nullopt;
}

inline Optional<unsigned> getUInt(const String& variable)
{
    auto string = get(variable);
    if (!string)
        return WTF::nullopt;

    bool ok;
    auto value = string->toUIntStrict(&ok);
    return ok ? makeOptional(value) : WTF::nullopt;
}

inline bool hasValue(const String& variable, const String& expected)
{
    auto actual = get(variable);
    return actual && *actual == expected;
}

inline bool hasValueOtherThan(const String& variable, const String& expected)
{
    auto actual = get(variable);
    return actual && *actual != expected;
}

WTF_EXPORT_PRIVATE void set(const String& variable, const String& value);
WTF_EXPORT_PRIVATE void setIfNotDefined(const String& variable, const String& value);

WTF_EXPORT_PRIVATE void remove(const String&);

} // namespace EnvironmentImpl
} // namespace WTF

namespace Environment = WTF::EnvironmentImpl;
