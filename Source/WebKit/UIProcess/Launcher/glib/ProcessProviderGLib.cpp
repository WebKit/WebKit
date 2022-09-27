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

namespace WebKit {

ProcessID ProcessProviderLibWPE::launchProcess(const ProcessLauncher::LaunchOptions& launchOptions, char** argv, int childProcessSocket)
{
#if WPE_CHECK_VERSION(1, 14, 0)
    UNUSED_PARAM(childProcessSocket);
    if (!m_provider)
        return -1;

    if (wpe_process_launch(m_provider.get(), wpeProcessType(launchOptions.processType), argv) > -1)
        return launchOptions.processIdentifier.toUInt64();
    return -1;
#else
    UNUSED_PARAM(launchOptions);
    UNUSED_PARAM(argv);
    UNUSED_PARAM(childProcessSocket);
    return -1;
#endif
}

} // namespace WebKit

#endif // USE(LIBWPE) && !ENABLE(BUBBLEWRAP_SANDBOX)
