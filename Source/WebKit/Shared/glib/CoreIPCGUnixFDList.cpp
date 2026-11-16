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

#include "config.h"
#include "CoreIPCGUnixFDList.h"

#if USE(GLIB)

#include <gio/gunixfdlist.h>

namespace WebKit {

CoreIPCGUnixFDList::CoreIPCGUnixFDList(const GRefPtr<GUnixFDList>& fdList)
    : m_fdList(fdList)
{
}

CoreIPCGUnixFDList::CoreIPCGUnixFDList(Vector<WTF::UnixFileDescriptor>&& fileDescriptors)
    : m_fdList(adoptGRef(g_unix_fd_list_new()))
{
    for (const auto& fileDescriptor : fileDescriptors)
        g_unix_fd_list_append(m_fdList.get(), fileDescriptor.value(), nullptr);
}

Vector<WTF::UnixFileDescriptor> CoreIPCGUnixFDList::fileDescriptors() const
{
    unsigned length = std::max(0, m_fdList ? g_unix_fd_list_get_length(m_fdList.get()) : 0);
    if (!length)
        return { };

    return Vector<WTF::UnixFileDescriptor>(length, [&](size_t i) {
        return WTF::UnixFileDescriptor { g_unix_fd_list_get(m_fdList.get(), i, nullptr), WTF::UnixFileDescriptor::Adopt };
    });
}

} // namespace WebKit

#endif // USE(GLIB)
