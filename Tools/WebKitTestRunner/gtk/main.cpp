/*
 * Copyright (C) 2011 Igalia S.L.
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

#include "TestController.h"
#include <WebKit/GtkVersioning.h>
#include <WebKit/WKTextCheckerGLib.h>
#include <wtf/glib/GRefPtr.h>

void initializeGtkSettings()
{
    GtkSettings* settings = gtk_settings_get_default();
    if (!settings)
        return;
    g_object_set(settings,
        "gtk-xft-dpi", 98304,
        "gtk-xft-antialias", 1,
        "gtk-font-name", "Liberation Sans 12",
        "gtk-xft-rgba", "none", nullptr);
}


int main(int argc, char** argv)
{
    g_setenv("WEBKIT_DISABLE_MEMORY_PRESSURE_MONITOR", "1", FALSE);
#if USE(ATSPI)
    g_setenv("WEBKIT_A11Y_BUS_ADDRESS", "", FALSE);
#if USE(GTK4)
    g_setenv("GTK_A11Y", "none", TRUE);
#else
    g_setenv("NO_AT_BRIDGE", "1", TRUE);
#endif
#endif


    gtk_init(&argc, &argv);

    initializeGtkSettings();

    GRefPtr<GPtrArray> languages = adoptGRef(g_ptr_array_new());
    g_ptr_array_add(languages.get(), const_cast<gpointer>(static_cast<const void*>("en_US")));
    WKTextCheckerSetSpellCheckingLanguages(reinterpret_cast<const char* const*>(languages->pdata), languages->len);

    // Prefer the not installed web and plugin processes.
    WTR::TestController controller(argc, const_cast<const char**>(argv));

    return 0;
}
