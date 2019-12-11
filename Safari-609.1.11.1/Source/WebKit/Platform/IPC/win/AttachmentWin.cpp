/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Konstantin Tokarev <annulen@yandex.ru>
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
#include "Attachment.h"

#include "Decoder.h"
#include "Encoder.h"

// FIXME: This code is duplicated with SharedMemory::Handle implementation for Win

namespace IPC {

void Attachment::encode(Encoder& encoder) const
{
    // Hand off ownership of our HANDLE to the receiving process. It will close it for us.
    // FIXME: If the receiving process crashes before it receives the memory, the memory will be
    // leaked. See <http://webkit.org/b/47502>.
    encoder << reinterpret_cast<uint64_t>(m_handle);

    // Send along our PID so that the receiving process can duplicate the HANDLE for its own use.
    encoder << static_cast<uint32_t>(::GetCurrentProcessId());
}

static bool getDuplicatedHandle(HANDLE sourceHandle, DWORD sourcePID, HANDLE& duplicatedHandle)
{
    duplicatedHandle = 0;
    if (!sourceHandle)
        return true;

    HANDLE sourceProcess = ::OpenProcess(PROCESS_DUP_HANDLE, FALSE, sourcePID);
    if (!sourceProcess)
        return false;

    // Copy the handle into our process and close the handle that the sending process created for us.
    BOOL success = ::DuplicateHandle(sourceProcess, sourceHandle, ::GetCurrentProcess(), &duplicatedHandle, 0, FALSE, DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
    ASSERT_WITH_MESSAGE(success, "::DuplicateHandle failed with error %lu", ::GetLastError());

    ::CloseHandle(sourceProcess);

    return success;
}

bool Attachment::decode(Decoder& decoder, Attachment& attachment)
{
    ASSERT_ARG(attachment, attachment.m_handle == INVALID_HANDLE_VALUE);

    uint64_t sourceHandle;
    if (!decoder.decode(sourceHandle))
        return false;

    uint32_t sourcePID;
    if (!decoder.decode(sourcePID))
        return false;

    HANDLE duplicatedHandle;
    if (!getDuplicatedHandle(reinterpret_cast<HANDLE>(sourceHandle), sourcePID, duplicatedHandle))
        return false;

    attachment.m_handle = duplicatedHandle;
    return true;
}

} // namespace IPC
