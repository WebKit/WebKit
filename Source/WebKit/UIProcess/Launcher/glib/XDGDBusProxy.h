/*
 * Copyright (C) 2018, 2022 Igalia S.L.
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

#if ENABLE(BUBBLEWRAP_SANDBOX)
#include "ProcessLauncher.h"
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/unix/UnixFileDescriptor.h>

namespace WebKit {

class XDGDBusProxy {
    WTF_MAKE_TZONE_ALLOCATED(XDGDBusProxy);
    WTF_MAKE_NONCOPYABLE(XDGDBusProxy);
public:
    XDGDBusProxy() = default;
    ~XDGDBusProxy() = default;

    enum class AllowPortals : bool { No, Yes };
    std::optional<CString> dbusSessionProxy(const char* baseDirectory, AllowPortals);
#if USE(ATSPI)
    std::optional<CString> accessibilityProxy(const char* baseDirectory, const String& sandboxedAccessibilityBusPath);
#endif

    bool launch(const ProcessLauncher::LaunchOptions&);

private:
    static CString makeProxy(const char* baseDirectory, const char* proxyTemplate);

    Vector<CString> m_args;
    CString m_dbusSessionProxyPath;
#if USE(ATSPI)
    CString m_accessibilityProxyPath;
#endif
    UnixFileDescriptor m_syncFD;
};

} // namespace WebKit

#endif // ENABLE(BUBBLEWRAP_SANDBOX)
