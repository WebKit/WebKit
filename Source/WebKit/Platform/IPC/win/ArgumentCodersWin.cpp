/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ArgumentCodersWin.h"

#include "Decoder.h"
#include "Encoder.h"
#include <wtf/win/Win32Handle.h>

namespace IPC {

void ArgumentCoder<Win32Handle>::encode(Encoder& encoder, Win32Handle&& handle)
{
    encoder << reinterpret_cast<uint64_t>(handle.leak());
    // Send along our PID so that the receiving process can duplicate the HANDLE for its own use.
    encoder << static_cast<uint32_t>(::GetCurrentProcessId());
}

std::optional<Win32Handle> ArgumentCoder<Win32Handle>::decode(Decoder& decoder)
{
    auto sourceHandle = decoder.decode<uint64_t>();
    auto sourcePID = decoder.decode<uint32_t>();
    if (UNLIKELY(!decoder.isValid()))
        return std::nullopt;
    if (reinterpret_cast<HANDLE>(*sourceHandle) == INVALID_HANDLE_VALUE)
        return Win32Handle { };
    auto sourceProcess = Win32Handle::adopt(::OpenProcess(PROCESS_DUP_HANDLE, FALSE, *sourcePID));
    if (!sourceProcess)
        return std::nullopt;
    HANDLE duplicatedHandle;
    // Copy the handle into our process and close the handle that the sending process created for us.
    if (!::DuplicateHandle(sourceProcess.get(), reinterpret_cast<HANDLE>(*sourceHandle), ::GetCurrentProcess(), &duplicatedHandle, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {
        ASSERT_WITH_MESSAGE(false, "::DuplicateHandle failed with error %lu", ::GetLastError());
        return std::nullopt;
    }
    return Win32Handle::adopt(duplicatedHandle);
}

}
