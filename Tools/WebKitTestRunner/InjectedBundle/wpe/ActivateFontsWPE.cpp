/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "ActivateFonts.h"

#include <fontconfig/fontconfig.h>
#include <wtf/glib/GLibUtilities.h>
#include <wtf/glib/GUniquePtr.h>

namespace WTR {

CString topLevelPath()
{
    if (const char* topLevelDirectory = g_getenv("WEBKIT_TOP_LEVEL"))
        return topLevelDirectory;

    // If the environment variable wasn't provided then assume we were built into
    // WebKitBuild/Debug or WebKitBuild/Release. Obviously this will fail if the build
    // directory is non-standard, but we can't do much more about this.
    GUniquePtr<char> parentPath(g_path_get_dirname(getCurrentExecutablePath().data()));
    GUniquePtr<char> layoutTestsPath(g_build_filename(parentPath.get(), "..", "..", "..", nullptr));
    GUniquePtr<char> absoluteTopLevelPath(realpath(layoutTestsPath.get(), 0));
    return absoluteTopLevelPath.get();
}

CString getOutputDir()
{
    const char* webkitOutputDir = g_getenv("WEBKIT_OUTPUTDIR");
    if (webkitOutputDir)
        return webkitOutputDir;

    GUniquePtr<char> outputDir(g_build_filename(topLevelPath().data(), "WebKitBuild", nullptr));
    return outputDir.get();
}

static CString getFontsPath()
{
    CString webkitOutputDir = getOutputDir();
    GUniquePtr<char> fontsPath(g_build_filename(webkitOutputDir.data(), "DependenciesWPE", "Root", "webkitgtk-test-fonts", nullptr));
    if (g_file_test(fontsPath.get(), static_cast<GFileTest>(G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
        return fontsPath.get();

    // Try alternative fonts path.
    fontsPath.reset(g_build_filename(webkitOutputDir.data(), "webkitgtk-test-fonts", NULL));
    if (g_file_test(fontsPath.get(), static_cast<GFileTest>(G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)))
        return fontsPath.get();

    return CString();
}

void activateFonts()
{
    if (g_getenv("WEBKIT_SKIP_WEBKITTESTRUNNER_FONTCONFIG_INITIALIZATION"))
        return;

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
    GUniquePtr<gchar> fontConfigFilename(g_build_filename(FONTS_CONF_DIR, "fonts.conf", nullptr));
    if (!g_file_test(fontConfigFilename.get(), G_FILE_TEST_IS_REGULAR))
        g_error("Cannot find fonts.conf at %s\n", fontConfigFilename.get());
    if (!FcConfigParseAndLoad(config, reinterpret_cast<FcChar8*>(fontConfigFilename.get()), true))
        g_error("Couldn't load font configuration file from: %s", fontConfigFilename.get());

    CString fontsPath = getFontsPath();
    if (fontsPath.isNull())
        g_error("Could not locate test fonts at %s. Is WEBKIT_TOP_LEVEL set?", fontsPath.data());

    GUniquePtr<GDir> fontsDirectory(g_dir_open(fontsPath.data(), 0, nullptr));
    while (const char* directoryEntry = g_dir_read_name(fontsDirectory.get())) {
        if (!g_str_has_suffix(directoryEntry, ".ttf") && !g_str_has_suffix(directoryEntry, ".otf"))
            continue;
        GUniquePtr<gchar> fontPath(g_build_filename(fontsPath.data(), directoryEntry, nullptr));
        if (!FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(fontPath.get())))
            g_error("Could not load font at %s!", fontPath.get());
    }

    // Ahem is used by many layout tests.
    GUniquePtr<gchar> ahemFontFilename(g_build_filename(FONTS_CONF_DIR, "AHEM____.TTF", nullptr));
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
        GUniquePtr<gchar> fontFilename(g_build_filename(FONTS_CONF_DIR, "..", "..", "fonts", fontFilenames[i], nullptr));
        if (!FcConfigAppFontAddFile(config, reinterpret_cast<FcChar8*>(fontFilename.get())))
            g_error("Could not load font at %s!", fontFilename.get()); 
    }

    // A font with no valid Fontconfig encoding to test https://bugs.webkit.org/show_bug.cgi?id=47452
    GUniquePtr<gchar> fontWithNoValidEncodingFilename(g_build_filename(FONTS_CONF_DIR, "FontWithNoValidEncoding.fon", nullptr));
    if (!FcConfigAppFontAddFile(config, reinterpret_cast<FcChar8*>(fontWithNoValidEncodingFilename.get())))
        g_error("Could not load font at %s!", fontWithNoValidEncodingFilename.get()); 

    if (!FcConfigSetCurrent(config))
        g_error("Could not set the current font configuration!");

    numFonts = FcConfigGetFonts(config, FcSetApplication)->nfont;
}

void installFakeHelvetica(WKStringRef)
{
}

void uninstallFakeHelvetica()
{
}

} // namespace WTR
