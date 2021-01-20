/*
 * Copyright (C) 2021 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RuntimeApplicationChecks.h"

#include <mutex>
#include <wtf/glib/GLibUtilities.h>

namespace WebCore {

#if PLATFORM(GTK)
static const CString webProcessName = "WebKitWebProcess";
static const CString networkProcessName = "WebKitNetworkProcess";
#elif PLATFORM(WPE)
static const CString webProcessName = "WPEWebProcess";
static const CString networkProcessName = "WPENetworkProcess";
#endif

bool isInWebProcess()
{
    static bool isWebProcess;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        isWebProcess = getCurrentExecutableName() == webProcessName;
    });
    return isWebProcess;
}

bool isInNetworkProcess()
{
    static bool isNetworkProcess;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        isNetworkProcess = getCurrentExecutableName() == networkProcessName;
    });
    return isNetworkProcess;
}

} // namespace WebCore
