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

#include "config.h"
#include <wtf/Environment.h>

#include <cstdlib>
#include <wtf/Assertions.h>
#include <wtf/Threading.h>

namespace WTF {
namespace EnvironmentImpl {

Optional<String> get(const String& variable)
{
    auto value = getenv(variable.utf8().data());
    return value ? makeOptional(String::fromUTF8(value)) : WTF::nullopt;
}

void set(const String& variable, const String& value)
{
    ASSERT(!WTF::threadingIsInitialized());
    setenv(variable.utf8().data(), value.utf8().data(), 1);
}

void setIfNotDefined(const String& variable, const String& value)
{
    ASSERT(!WTF::threadingIsInitialized());
    setenv(variable.utf8().data(), value.utf8().data(), 0);
}

void remove(const String& variable)
{
    ASSERT(!WTF::threadingIsInitialized());
    unsetenv(variable.utf8().data());
}

} // namespace EnvironmentImpl
} // namespace WTF
