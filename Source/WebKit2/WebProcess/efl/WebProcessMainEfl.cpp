/*
 * Copyright (C) 2011 Samsung Electronics. All rights reserved.
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
#include "WebProcessMainEfl.h"

#include "ProxyResolverSoup.h"
#include "WKBase.h"
#include <Ecore.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/RunLoop.h>
#include <WebKit2/WebProcess.h>
#include <runtime/InitializeThreading.h>
#include <unistd.h>
#include <wtf/MainThread.h>

#if USE(COORDINATED_GRAPHICS)
#include "CoordinatedGraphicsLayer.h"
#endif

using namespace WebCore;

namespace WebKit {

WK_EXPORT int WebProcessMainEfl(int argc, char* argv[])
{
    // WebProcess should be launched with an option.
    if (argc != 2)
        return 1;

    if (!eina_init())
        return 1;

    if (!ecore_init()) {
        // Could not init ecore.
        eina_shutdown();
        return 1;
    }

#if ENABLE(GLIB_SUPPORT)
    g_type_init();

    if (!ecore_main_loop_glib_integrate())
        return 1;
#endif

    JSC::initializeThreading();
    WTF::initializeMainThread();

    RunLoop::initializeMainRunLoop();

    SoupSession* session = WebCore::ResourceHandle::defaultSession();
    const char* httpProxy = getenv("http_proxy");
    if (httpProxy) {
        const char* noProxy = getenv("no_proxy");
        SoupProxyURIResolver* resolverEfl = soupProxyResolverWkNew(httpProxy, noProxy);
        soup_session_add_feature(session, SOUP_SESSION_FEATURE(resolverEfl));
        g_object_unref(resolverEfl);
    }

#if USE(COORDINATED_GRAPHICS)
    CoordinatedGraphicsLayer::initFactory();
#endif

    WebCore::ResourceHandle::setIgnoreSSLErrors(true);

    int socket = atoi(argv[1]);
    WebProcess::shared().initialize(socket, RunLoop::main());
    RunLoop::run();

    return 0;

}

} // namespace WebKit
