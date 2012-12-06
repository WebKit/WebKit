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

#define LIBSOUP_USE_UNSTABLE_REQUEST_API

#include "ProxyResolverSoup.h"
#include "WKBase.h"
#include <Ecore.h>
#include <Efreet.h>
#include <WebCore/ResourceHandle.h>
#include <WebCore/RunLoop.h>
#include <WebKit2/WebProcess.h>
#include <libsoup/soup-cache.h>
#include <runtime/InitializeThreading.h>
#include <unistd.h>
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>

#ifdef HAVE_ECORE_X
#include <Ecore_X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xext.h>

static int dummyExtensionErrorHandler(Display*, _Xconst char*, _Xconst char*)
{
    return 0;
}
#endif

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

#ifdef HAVE_ECORE_X
    XSetExtensionErrorHandler(dummyExtensionErrorHandler);

    if (!ecore_x_init(0)) {
        // Could not init ecore_x.
        // PlatformScreenEfl and systemBeep() functions
        // depend on ecore_x functionality.
        ecore_shutdown();
        eina_shutdown();
        return 1;
    }
#endif

    g_type_init();

    if (!ecore_main_loop_glib_integrate())
        return 1;

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

    // Set SOUP cache.
    String soupCacheDirectory = String::fromUTF8(efreet_cache_home_get()) + "/WebKitEfl";
    SoupCache* soupCache = soup_cache_new(soupCacheDirectory.utf8().data(), SOUP_CACHE_SINGLE_USER);
    soup_session_add_feature(session, SOUP_SESSION_FEATURE(soupCache));
    soup_cache_load(soupCache);

#if USE(COORDINATED_GRAPHICS)
    CoordinatedGraphicsLayer::initFactory();
#endif

    WebCore::ResourceHandle::setIgnoreSSLErrors(true);

    int socket = atoi(argv[1]);
    WebProcess::shared().initialize(socket, RunLoop::main());
    RunLoop::run();

    soup_cache_flush(soupCache);
    soup_cache_dump(soupCache);
    g_object_unref(soupCache);

    ecore_x_shutdown();
    ecore_shutdown();
    eina_shutdown();

    return 0;

}

} // namespace WebKit
