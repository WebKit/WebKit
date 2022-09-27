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

#pragma once

#if USE(LIBWPE) && !ENABLE(BUBBLEWRAP_SANDBOX)

#include "ProcessLauncher.h"
#include <wtf/ProcessID.h>

struct wpe_process_provider;

namespace WebKit {

class ProcessProviderLibWPE {
    WTF_MAKE_NONCOPYABLE(ProcessProviderLibWPE);
    friend class NeverDestroyed<ProcessProviderLibWPE>;

public:
    static ProcessProviderLibWPE& singleton();

    bool isEnabled();

    ProcessID launchProcess(const ProcessLauncher::LaunchOptions&, char** argv, int childProcessSocket);

    void kill(ProcessID);

private:
    ProcessProviderLibWPE();

    std::unique_ptr<struct wpe_process_provider, void (*)(struct wpe_process_provider*)> m_provider;

    static int wpeProcessType(ProcessLauncher::ProcessType);
};

} // namespace WebKit

#endif // USE(LIBWPE) && !ENABLE(BUBBLEWRAP_SANDBOX)
