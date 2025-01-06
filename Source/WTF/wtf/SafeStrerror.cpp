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

#include <cstring>
#include <type_traits>
#include <wtf/Platform.h>
#include <wtf/text/CString.h>

namespace WTF {

CString safeStrerror(int errnum)
{
    constexpr size_t bufferLength = 1024;
    std::span<char> cstringBuffer;
    auto result = CString::newUninitialized(bufferLength, cstringBuffer);
#if OS(WINDOWS)
    strerror_s(cstringBuffer.data(), cstringBuffer.size(), errnum);
#else
    auto ret = strerror_r(errnum, cstringBuffer.data(), cstringBuffer.size());
    if constexpr (std::is_same<decltype(ret), char*>::value) {
        // We have GNU strerror_r(), which returns char*. This may or may not be a pointer into
        // cstringBuffer. We also have to be careful because this has to compile even if ret is
        // an int, hence the reinterpret_casts.
        char* message = reinterpret_cast<char*>(ret);
        if (message != cstringBuffer.data())
            strncpy(cstringBuffer.data(), message, cstringBuffer.size());
    } else {
        // We have POSIX strerror_r, which returns int and may fail.
        if (ret)
            snprintf(cstringBuffer.data(), cstringBuffer.size(), "%s %d", "Unknown error", errnum);
    }
#endif // OS(WINDOWS)
    return result;
}

} // namespace WTF
