/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "ArgumentCoders.h"
#include "Attachment.h"
#include "Decoder.h"
#include "Encoder.h"
#include "SharedFileHandle.h"
#include <wtf/MachSendRight.h>

#include <pal/spi/cocoa/FilePortSPI.h>

namespace IPC {

std::optional<SharedFileHandle> SharedFileHandle::create(FileSystem::PlatformFileHandle handle)
{
    return SharedFileHandle { handle };
}

SharedFileHandle::SharedFileHandle(MachSendRight&& fileport)
{
    int fd = fileport_makefd(fileport.sendRight());
    if (fd == -1)
        return;
    m_handle = WebCore::FileHandle { fd };
}

MachSendRight SharedFileHandle::toMachSendRight() const
{
    mach_port_name_t fileport = MACH_PORT_NULL;
    if (fileport_makeport(m_handle.handle(), &fileport) == -1)
        return { };

    return MachSendRight::adopt(fileport);
}

} // namespace IPC
