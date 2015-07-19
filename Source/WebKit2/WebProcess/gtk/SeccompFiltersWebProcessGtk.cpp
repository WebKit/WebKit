/*
 * Copyright (C) 2013 Intel Corporation. All rights reserved.
 * Copyright (C) 2015 Igalia S.L.
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
#include "SeccompFiltersWebProcessGtk.h"

#if ENABLE(SECCOMP_FILTERS)

#include "SeccompBroker.h"
#include "WebProcessCreationParameters.h"
#include <glib.h>
#include <sys/types.h>
#include <unistd.h>

namespace WebKit {

SeccompFiltersWebProcessGtk::SeccompFiltersWebProcessGtk(const WebProcessCreationParameters& parameters)
    : SeccompFilters(Allow)
{
    m_policy.addDefaultWebProcessPolicy(parameters);
}

void SeccompFiltersWebProcessGtk::platformInitialize()
{
    // TODO: We should block all the syscalls and whitelist
    // what we need + trap what should be handled by the broker.
    addRule("open", Trap);
    addRule("openat", Trap);
    addRule("creat", Trap);

#if USE(GSTREAMER)
    m_policy.addDirectoryPermission(String::fromUTF8(g_get_user_cache_dir()) + "/gstreamer-1.0", SyscallPolicy::ReadAndWrite);
    m_policy.addDirectoryPermission(String::fromUTF8(g_get_user_data_dir()) + "/gstreamer-1.0", SyscallPolicy::ReadAndWrite);
    m_policy.addDirectoryPermission(String::fromUTF8(LIBEXECDIR) + "/gstreamer-1.0", SyscallPolicy::Read);
#endif

    m_policy.addDirectoryPermission(String::fromUTF8(g_get_user_data_dir()) + "/gvfs-metadata", SyscallPolicy::ReadAndWrite);

    // For libXau
    m_policy.addDirectoryPermission(ASCIILiteral("/run/gdm"), SyscallPolicy::Read);

    SeccompBroker::launchProcess(this, m_policy);
}

} // namespace WebKit

#endif // ENABLE(SECCOMP_FILTERS)
