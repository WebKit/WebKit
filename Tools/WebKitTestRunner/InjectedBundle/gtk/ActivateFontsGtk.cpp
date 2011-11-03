/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2010 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ActivateFonts.h"

#include <fontconfig/fontconfig.h>
#include <gtk/gtk.h>
#include <wtf/gobject/GOwnPtr.h>

namespace WTR {

void initializeGtkSettings()
{
    GtkSettings* settings = gtk_settings_get_default();
    if (!settings)
        return;
    g_object_set(settings, 
                 "gtk-xft-dpi", 98304,
                 "gtk-xft-antialias", 1,
                 "gtk-xft-hinting", 0,
                 "gtk-font-name", "Liberation Sans 12",
                 "gtk-theme-name", "Raleigh",
                 "gtk-xft-rgba", "none", NULL);
}

void inititializeFontConfigSetting()
{
    FcInit();

    // If a test resulted a font being added or removed via the @font-face rule, then
    // we want to reset the FontConfig configuration to prevent it from affecting other tests.
    static int numFonts = 0;
    FcFontSet* appFontSet = FcConfigGetFonts(0, FcSetApplication);
    if (appFontSet && numFonts && appFontSet->nfont == numFonts)
        return;

    // Load our configuration file, which sets up proper aliases for family
    // names like sans, serif and monospace.
    FcConfig* config = FcConfigCreate();
    GOwnPtr<gchar> fontConfigFilename(g_build_filename(FONTS_CONF_DIR, "fonts.conf", NULL));
    if (!g_file_test(fontConfigFilename.get(), G_FILE_TEST_IS_REGULAR))
        g_error("Cannot find fonts.conf at %s\n", fontConfigFilename.get());
    if (!FcConfigParseAndLoad(config, reinterpret_cast<FcChar8*>(fontConfigFilename.get()), true))
        g_error("Couldn't load font configuration file from: %s", fontConfigFilename.get());

    static const char *const fontDirectories[] = {
        "/usr/share/fonts/truetype/liberation",
        "/usr/share/fonts/truetype/ttf-liberation",
        "/usr/share/fonts/liberation",
        "/usr/share/fonts/truetype/ttf-dejavu",
        "/usr/share/fonts/dejavu",
        "/usr/share/fonts/opentype/stix",
        "/usr/share/fonts/stix"
    };

    static const char *const fontPaths[] = {
        "LiberationMono-BoldItalic.ttf",
        "LiberationMono-Bold.ttf",
        "LiberationMono-Italic.ttf",
        "LiberationMono-Regular.ttf",
        "LiberationSans-BoldItalic.ttf",
        "LiberationSans-Bold.ttf",
        "LiberationSans-Italic.ttf",
        "LiberationSans-Regular.ttf",
        "LiberationSerif-BoldItalic.ttf",
        "LiberationSerif-Bold.ttf",
        "LiberationSerif-Italic.ttf",
        "LiberationSerif-Regular.ttf",
        "DejaVuSans.ttf",
        "DejaVuSerif.ttf",

        // MathML tests require the STIX fonts.
        "STIXGeneral.otf",
        "STIXGeneralBolIta.otf",
        "STIXGeneralBol.otf",
        "STIXGeneralItalic.otf"
    };

    // TODO: Some tests use Lucida. We should load these as well, once it becomes
    // clear how to install these fonts easily on Fedora.
    for (size_t font = 0; font < G_N_ELEMENTS(fontPaths); font++) {
        bool found = false;
        for (size_t path = 0; path < G_N_ELEMENTS(fontDirectories); path++) {
            GOwnPtr<gchar> fullPath(g_build_filename(fontDirectories[path], fontPaths[font], NULL));
            if (g_file_test(fullPath.get(), G_FILE_TEST_EXISTS)) {
                found = true;
                if (!FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(fullPath.get())))
                    g_error("Could not load font at %s!", fullPath.get());
                else
                    break;
            }
        }

        if (!found) {
            GOwnPtr<gchar> directoriesDescription;
            for (size_t path = 0; path < G_N_ELEMENTS(fontDirectories); path++)
                directoriesDescription.set(g_strjoin(":", directoriesDescription.release(), fontDirectories[path], NULL));
            g_error("Could not find font %s in %s. Either install this font or file a bug "
                    "at http://bugs.webkit.org if it is installed in another location.",
                    fontPaths[font], directoriesDescription.get());
        }
    }

    // Ahem is used by many layout tests.
    GOwnPtr<gchar> ahemFontFilename(g_build_filename(FONTS_CONF_DIR, "AHEM____.TTF", NULL));
    if (!FcConfigAppFontAddFile(config, reinterpret_cast<FcChar8*>(ahemFontFilename.get())))
        g_error("Could not load font at %s!", ahemFontFilename.get()); 

    static const char* fontFilenames[] = {
        "WebKitWeightWatcher100.ttf",
        "WebKitWeightWatcher200.ttf",
        "WebKitWeightWatcher300.ttf",
        "WebKitWeightWatcher400.ttf",
        "WebKitWeightWatcher500.ttf",
        "WebKitWeightWatcher600.ttf",
        "WebKitWeightWatcher700.ttf",
        "WebKitWeightWatcher800.ttf",
        "WebKitWeightWatcher900.ttf",
        0
    };

    for (size_t i = 0; fontFilenames[i]; ++i) {
        GOwnPtr<gchar> fontFilename(g_build_filename(FONTS_CONF_DIR, "..", "..", "fonts", fontFilenames[i], NULL));
        if (!FcConfigAppFontAddFile(config, reinterpret_cast<FcChar8*>(fontFilename.get())))
            g_error("Could not load font at %s!", fontFilename.get()); 
    }

    // A font with no valid Fontconfig encoding to test https://bugs.webkit.org/show_bug.cgi?id=47452
    GOwnPtr<gchar> fontWithNoValidEncodingFilename(g_build_filename(FONTS_CONF_DIR, "FontWithNoValidEncoding.fon", NULL));
    if (!FcConfigAppFontAddFile(config, reinterpret_cast<FcChar8*>(fontWithNoValidEncodingFilename.get())))
        g_error("Could not load font at %s!", fontWithNoValidEncodingFilename.get()); 

    if (!FcConfigSetCurrent(config))
        g_error("Could not set the current font configuration!");

    numFonts = FcConfigGetFonts(config, FcSetApplication)->nfont;
}

void activateFonts()
{
    initializeGtkSettings();
    inititializeFontConfigSetting();
}

}
