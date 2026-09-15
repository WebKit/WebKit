/* 
 * Copyright (C) 2021 Red Hat Inc.
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

#include "config.h"
#include <wtf/SafeStrerror.h>

#include <array>
#include <cstring>
#include <type_traits>
#include <wtf/Platform.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>

namespace WTF {

UTF8CString safeStrerror(int errnum)
{
    constexpr size_t bufferLength = 1024;
    std::array<char, bufferLength> buffer;
    bool unknownError = false;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

    const char* message = buffer.data();

#if OS(WINDOWS)
    strerror_s(buffer.data(), buffer.size(), errnum);
#else
    auto ret = strerror_r(errnum, buffer.data(), buffer.size());

    if constexpr (std::is_same<decltype(ret), char*>::value) {
        // We have GNU strerror_r(), which returns char*. This may or may not be a pointer into
        // buffer, and either way it is null-terminated. We also have to be careful because this
        // has to compile even if ret is an int, hence the reinterpret_cast.
        message = reinterpret_cast<const char*>(ret);
    } else {
        // We have POSIX strerror_r, which returns int and may fail.
        unknownError = !!ret;
    }
#endif // OS(WINDOWS)

    // strerror_r() leaves the buffer unspecified when it fails.
    if (unknownError)
        return makeString("Unknown error "_s, errnum).utf8();

    // strerror_r() truncates rather than overflowing, so bound the length by the buffer.
    size_t length = strnlen(message, bufferLength);
    auto messageSpan = unsafeMakeSpan(message, length);

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    return UTF8CString { byteCast<char8_t>(messageSpan) };
}

} // namespace WTF
