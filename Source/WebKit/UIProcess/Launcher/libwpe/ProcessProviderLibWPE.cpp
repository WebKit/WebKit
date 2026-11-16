/*
 * Copyright (C) 2022 Igalia S.L.
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
#include "ProcessProviderLibWPE.h"

#if USE(LIBWPE) && !ENABLE(BUBBLEWRAP_SANDBOX)

#include <wpe/wpe.h>
#include <wtf/NeverDestroyed.h>

namespace WebKit {

int ProcessProviderLibWPE::wpeProcessType(ProcessLauncher::ProcessType processType)
{
#if WPE_CHECK_VERSION(1, 14, 0)
    switch (processType) {
    case ProcessLauncher::ProcessType::Network:
        return WPE_PROCESS_TYPE_NETWORK;
#if ENABLE(GPU_PROCESS)
    case ProcessLauncher::ProcessType::GPU:
        return WPE_PROCESS_TYPE_GPU;
#endif
    case ProcessLauncher::ProcessType::Web:
    default:
        return WPE_PROCESS_TYPE_WEB;
    }
#else
    UNUSED_PARAM(processType);
    ASSERT_NOT_REACHED();
    return -1;
#endif
}

ProcessProviderLibWPE& ProcessProviderLibWPE::singleton()
{
    static NeverDestroyed<ProcessProviderLibWPE> sharedProvider;
    return sharedProvider;
}

ProcessProviderLibWPE::ProcessProviderLibWPE()
#if WPE_CHECK_VERSION(1, 14, 0)
    : m_provider(wpe_process_provider_create(), wpe_process_provider_destroy)
#else
    : m_provider(nullptr, nullptr)
#endif
{
}

bool ProcessProviderLibWPE::isEnabled()
{
    return m_provider.get();
}

void ProcessProviderLibWPE::kill(ProcessID processID)
{
#if WPE_CHECK_VERSION(1, 14, 0)
    if (!m_provider)
        return;

    wpe_process_terminate(m_provider.get(), processID);
#else
    UNUSED_PARAM(processID);
#endif
}

} // namespace WebKit

#endif // USE(LIBWPE) && !ENABLE(BUBBLEWRAP_SANDBOX)
