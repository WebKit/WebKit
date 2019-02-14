/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#include "AuxiliaryProcessMain.h"
#include "WebProcess.h"
#include <WebCore/SoupNetworkSession.h>
#include <gtk/gtk.h>
#include <libintl.h>
#include <libsoup/soup.h>
#include <wtf/Environment.h>

#if PLATFORM(X11)
#include <X11/Xlib.h>
#endif

namespace WebKit {
using namespace WebCore;

class WebProcessMain final: public AuxiliaryProcessMainBase {
public:
    bool platformInitialize() override
    {
#ifndef NDEBUG
        if (Environment::get("WEBKIT2_PAUSE_WEB_PROCESS_ON_LAUNCH"))
            g_usleep(30 * G_USEC_PER_SEC);
#endif

#if (USE(COORDINATED_GRAPHICS_THREADED) || USE(GSTREAMER_GL)) && PLATFORM(X11)
        XInitThreads();
#endif
        gtk_init(nullptr, nullptr);

        bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
        bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

        return true;
    }
};

int WebProcessMainUnix(int argc, char** argv)
{
    return AuxiliaryProcessMain<WebProcess, WebProcessMain>(argc, argv);
}

} // namespace WebKit
