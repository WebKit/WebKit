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
#include "WebProcessMainUnix.h"

#include "ChildProcessMain.h"
#include "WebProcess.h"
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Edje.h>
#include <Efreet.h>
#include <WebCore/SoupNetworkSession.h>
#include <libsoup/soup.h>

#if HAVE(ACCESSIBILITY)
#include "Module.h"
/*
 * This should be defined and filled in elementary but to avoid undefined reference it's defined here.
 * Setting this variable to nullptr will not cause any trouble to webkit and eail as well.
 */
Eina_List* _elm_win_list = nullptr;

typedef int (*EailFunction)(void*);

static char* eailLibraryPath()
{
    return getenv("ACCESSIBILITY_EAIL_LIBRARY_PATH");
}

static WebKit::Module& eail()
{
    static NeverDestroyed<WebKit::Module> eail(eailLibraryPath());
    return eail;
}
#endif

#ifdef HAVE_ECORE_X
#include <Ecore_X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xext.h>

static int dummyExtensionErrorHandler(Display*, _Xconst char*, _Xconst char*)
{
    return 0;
}
#endif

using namespace WebCore;

namespace WebKit {

class WebProcessMain final: public ChildProcessMainBase {
public:
    bool platformInitialize() override
    {
        if (!eina_init())
            return false;

        if (!ecore_init()) {
            // Could not init ecore.
            eina_shutdown();
            return false;
        }

#ifdef HAVE_ECORE_X
        XSetExtensionErrorHandler(dummyExtensionErrorHandler);

        if (!ecore_x_init(0)) {
            // Could not init ecore_x.
            // PlatformScreenEfl and systemBeep() functions
            // depend on ecore_x functionality.
            ecore_shutdown();
            eina_shutdown();
            return false;
        }
#endif

        if (!ecore_evas_init()) {
#ifdef HAVE_ECORE_X
            ecore_x_shutdown();
#endif
            ecore_shutdown();
            eina_shutdown();
            return false;
        }

        if (!edje_init()) {
            ecore_evas_shutdown();
#ifdef HAVE_ECORE_X
            ecore_x_shutdown();
#endif
            ecore_shutdown();
            eina_shutdown();
            return false;
        }

        if (!ecore_main_loop_glib_integrate())
            return false;

#if HAVE(ACCESSIBILITY)
        // Initialize EAIL module (adding listeners, init atk-bridge)
        if (eailLibraryPath() && eail().load()) {
            if (EailFunction eailInit = eail().functionPointer<EailFunction>("elm_modapi_init"))
                eailInit(nullptr);
        }
#endif

        SoupNetworkSession::defaultSession().setupHTTPProxyFromEnvironment();
        return true;
    }

    void platformFinalize() override
    {
        if (SoupCache* soupCache = SoupNetworkSession::defaultSession().cache()) {
            soup_cache_flush(soupCache);
            soup_cache_dump(soupCache);
        }

        edje_shutdown();
        ecore_evas_shutdown();
#ifdef HAVE_ECORE_X
        ecore_x_shutdown();
#endif
        ecore_shutdown();
        eina_shutdown();

#if HAVE(ACCESSIBILITY)
        if (eailLibraryPath()) {
            if (EailFunction eailShutdown = eail().functionPointer<EailFunction>("elm_modapi_shutdown"))
                eailShutdown(nullptr);
        }
#endif
    }
};

int WebProcessMainUnix(int argc, char** argv)
{
    return ChildProcessMain<WebProcess, WebProcessMain>(argc, argv);
}

} // namespace WebKit
