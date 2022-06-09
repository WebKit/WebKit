/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#include <wtf/UniStdExtras.h>

namespace IPC {

Attachment::Attachment(int fileDescriptor, size_t size)
    : m_type(MappedMemoryType)
    , m_fileDescriptor(fileDescriptor)
    , m_size(size)
{
}

Attachment::Attachment(int fileDescriptor)
    : m_type(SocketType)
    , m_fileDescriptor(fileDescriptor)
    , m_size(0)
{
}

Attachment::Attachment(Attachment&& attachment)
    : m_type(attachment.m_type)
    , m_fileDescriptor(attachment.m_fileDescriptor)
    , m_size(attachment.m_size)
    , m_customWriter(WTFMove(attachment.m_customWriter))
{
    attachment.m_type = Uninitialized;
    attachment.m_fileDescriptor = -1;
    attachment.m_size = 0;
}

Attachment::Attachment(CustomWriter&& writer)
    : m_type(CustomWriterType)
    , m_fileDescriptor(-1)
    , m_size(0)
    , m_customWriter(WTFMove(writer))
{
}

Attachment& Attachment::operator=(Attachment&& attachment)
{
    m_type = attachment.m_type;
    attachment.m_type = Uninitialized;
    m_fileDescriptor = attachment.m_fileDescriptor;
    attachment.m_fileDescriptor = -1;
    m_size = attachment.m_size;
    attachment.m_size = 0;
    m_customWriter = WTFMove(attachment.m_customWriter);

    return *this;
}

Attachment::~Attachment()
{
    if (m_fileDescriptor != -1)
        closeWithRetry(m_fileDescriptor);
}

} // namespace IPC
