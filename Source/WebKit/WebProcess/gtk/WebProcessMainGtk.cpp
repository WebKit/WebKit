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
#include "WebProcessMain.h"

#include "AuxiliaryProcessMain.h"
#include "WebProcess.h"
#include <WebCore/GtkVersioning.h>
#include <libintl.h>

#if USE(GSTREAMER)
#include <WebCore/GStreamerCommon.h>
#endif

#if USE(GCRYPT)
#include <pal/crypto/gcrypt/Initialization.h>
#endif

#if USE(SKIA)
#include <skia/core/SkGraphics.h>
#endif

#if USE(SYSPROF_CAPTURE)
#include <wtf/SystemTracing.h>
#endif

namespace WebKit {
using namespace WebCore;

class WebProcessMainGtk final: public AuxiliaryProcessMainBase<WebProcess> {
public:
    bool platformInitialize() override
    {
#if USE(SYSPROF_CAPTURE)
        SysprofAnnotator::createIfNeeded("WebKit (Web)"_s);
#endif

#if USE(GCRYPT)
        PAL::GCrypt::initialize();
#endif

#if USE(SKIA)
        SkGraphics::Init();
#endif

#if ENABLE(DEVELOPER_MODE)
        if (g_getenv("WEBKIT2_PAUSE_WEB_PROCESS_ON_LAUNCH"))
            g_usleep(30 * G_USEC_PER_SEC);
#endif

        gtk_init(nullptr, nullptr);

        bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
        bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

        return true;
    }

    void platformFinalize() override
    {
#if USE(GSTREAMER)
        deinitializeGStreamer();
#endif
    }
};

int WebProcessMain(int argc, char** argv)
{
#if USE(ATSPI)
    // Disable ATK/GTK accessibility support in the WebProcess.
#if USE(GTK4)
    g_setenv("GTK_A11Y", "none", TRUE);
#else
    g_setenv("NO_AT_BRIDGE", "1", TRUE);
#endif
#endif

    // Ignore the GTK_THEME environment variable, the theme is always set by the UI process now.
    // This call needs to happen before any threads begin execution
    unsetenv("GTK_THEME");

    return AuxiliaryProcessMain<WebProcessMainGtk>(argc, argv);
}

} // namespace WebKit
