/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Company 100 Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS''
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
#include "NetworkProcessMainUnix.h"

#if ENABLE(NETWORK_PROCESS)

#include "WKBase.h"
#include "WebKit2Initialize.h"
#include <WebCore/SoupNetworkSession.h>
#include <WebKit2/NetworkProcess.h>
#include <runtime/InitializeThreading.h>
#include <stdlib.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/gobject/GRefPtr.h>

#if PLATFORM(EFL)
#include <Ecore.h>
#endif

#if USE(SOUP)
#include <libsoup/soup.h>
#endif

using namespace WebCore;

namespace WebKit {

WK_EXPORT int NetworkProcessMain(int argc, char* argv[])
{
    if (argc != 2)
        return 1;

#if PLATFORM(EFL)
    if (!ecore_init())
        return 1;

    if (!ecore_main_loop_glib_integrate())
        return 1;
#endif

    InitializeWebKit2();

    SoupNetworkSession::defaultSession().setupHTTPProxyFromEnvironment();

    int socket = atoi(argv[1]);

    WebKit::ChildProcessInitializationParameters parameters;
    parameters.connectionIdentifier = int(socket);

    NetworkProcess::shared().initialize(parameters);

#if USE(SOUP)
    // Despite using system CAs to validate certificates we're
    // accepting invalid certificates by default. New API will be
    // added later to let client accept/discard invalid certificates.
    SoupNetworkSession::defaultSession().setSSLPolicy(SoupNetworkSession::SSLUseSystemCAFile);
#endif

    RunLoop::run();

#if USE(SOUP)
    if (SoupCache* soupCache = SoupNetworkSession::defaultSession().cache()) {
        soup_cache_flush(soupCache);
        soup_cache_dump(soupCache);
    }
#endif

    return 0;
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
