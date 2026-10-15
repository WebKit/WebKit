/*
 * Copyright (C) 2026 Igalia S.L.
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

#include <JavaScriptCore/CorpsePlatform.h>

#if ENABLE(MYA_HEAP)

#include <array>
#include <string_view>

#if OS(DARWIN)
#include <CoreFoundation/CFRunLoop.h>
#endif

namespace JSC {
namespace Corpse {

// The compiler's spelling of T, from this function's signature: "[T = ...]" from clang, "[with T = ...]" from gcc.
template<typename T> constexpr std::string_view declaredTypeName()
{
    std::string_view function = __PRETTY_FUNCTION__;
    size_t start = function.find("T = ") + sizeof("T = ") - 1;
    size_t end = function.find_first_of(";]", start);
    return function.substr(start, end - start);
}

// Classes that no image of a WebKit process defines, such as handles to objects of system libraries. The heap walk
// follows a pointer to any other class that is only declared where it is held, by finding the object through an
// image that defines the class, and reports the object when none does. Each entry is the type itself, so that it
// is checked against the header that declares it.
#if OS(DARWIN)
constexpr std::array ignoredDeclarations {
    declaredTypeName<__CFRunLoop>(),
    declaredTypeName<__CFRunLoopSource>(),
};
#else
constexpr std::array<std::string_view, 0> ignoredDeclarations { };
#endif

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA_HEAP)
