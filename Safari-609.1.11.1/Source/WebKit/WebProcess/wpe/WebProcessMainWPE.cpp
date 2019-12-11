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
#include <glib.h>

#if ENABLE(ACCESSIBILITY)
#include <atk-bridge.h>
#include <atk/atk.h>
#endif

namespace WebKit {
using namespace WebCore;

#if ENABLE(ACCESSIBILITY)
static void initializeAccessibility()
{
    auto* atkUtilClass = ATK_UTIL_CLASS(g_type_class_ref(ATK_TYPE_UTIL));

    atkUtilClass->add_key_event_listener = [](AtkKeySnoopFunc, gpointer) -> guint {
        return 0;
    };

    atkUtilClass->remove_key_event_listener = [](guint) {
    };

    atkUtilClass->get_root = []() -> AtkObject* {
        // ATK bridge needs a root object. We use an AtkPlug because that way the
        // web process is not registered as an application.
        static AtkObject* root = nullptr;
        if (!root)
            root = atk_plug_new();
        return root;
    };

    atkUtilClass->get_toolkit_name = []() -> const gchar* {
        return "WPEWebKit";
    };

    atkUtilClass->get_toolkit_version = []() -> const gchar* {
        return "";
    };

    atk_bridge_adaptor_init(nullptr, nullptr);
}
#endif

class WebProcessMain final : public AuxiliaryProcessMainBase {
public:
    bool platformInitialize() override
    {
#if ENABLE(DEVELOPER_MODE)
        if (g_getenv("WEBKIT2_PAUSE_WEB_PROCESS_ON_LAUNCH"))
            WTF::sleep(30_s);
#endif

        // Required for GStreamer initialization.
        // FIXME: This should be probably called in other processes as well.
        g_set_prgname("WPEWebProcess");

#if ENABLE(ACCESSIBILITY)
        initializeAccessibility();
#endif

        return true;
    }
};

int WebProcessMainUnix(int argc, char** argv)
{
    return AuxiliaryProcessMain<WebProcess, WebProcessMain>(argc, argv);
}

} // namespace WebKit
