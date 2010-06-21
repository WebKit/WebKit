/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef InjectedBundle_h
#define InjectedBundle_h

#include "WKBundle.h"
#include <WebCore/PlatformString.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebKit {

#if PLATFORM(MAC)
typedef CFBundleRef PlatformBundle;
#elif PLATFORM(WIN)
typedef HMODULE PlatformBundle;
#endif

class WebPage;

class InjectedBundle : public RefCounted<InjectedBundle> {
public:
    static PassRefPtr<InjectedBundle> create(const WebCore::String& path)
    {
        return adoptRef(new InjectedBundle(path));
    }
    ~InjectedBundle();

    bool load();

    // API
    void initializeClient(WKBundleClient*);
    void postMessage(WebCore::StringImpl*);

    // Callback hooks
    void didCreatePage(WebPage*);
    void didRecieveMessage(const WebCore::String&);

private:
    InjectedBundle(const WebCore::String&);

    WebCore::String m_path;
    PlatformBundle m_platformBundle; // This is leaked right now, since we never unload the bundle/module.

    WKBundleClient m_client;
};

} // namespace WebKit

#endif // InjectedBundle_h
