/*
 * Copyright (C) 2011 ProFUSION Embedded Systems
 * Copyright (C) 2011 Samsung Electronics
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontManagement.h"

#include <Ecore_File.h>
#include <cstdio>
#include <fontconfig/fontconfig.h>
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

static Vector<String> getFontFiles()
{
    Vector<String> fontFilePaths;

    // Ahem is used by many layout tests.
    fontFilePaths.append(String(FONTS_CONF_DIR "/AHEM____.TTF"));
    // A font with no valid Fontconfig encoding to test https://bugs.webkit.org/show_bug.cgi?id=47452
    fontFilePaths.append(String(FONTS_CONF_DIR "/FontWithNoValidEncoding.fon"));

    for (int i = 1; i <= 9; i++) {
        char fontPath[PATH_MAX];
        snprintf(fontPath, PATH_MAX - 1,
                 FONTS_CONF_DIR "/../../fonts/WebKitWeightWatcher%i00.ttf", i);

        fontFilePaths.append(String(fontPath));
    }

    return fontFilePaths;
}

static bool addFontDirectory(const CString& fontDirectory, FcConfig* config)
{
    const char* path = fontDirectory.data();

    if (!ecore_file_is_dir(path)
        || !FcConfigAppFontAddDir(config, reinterpret_cast<const FcChar8*>(path))) {
        fprintf(stderr, "Could not add font directory %s!\n", path);
        return false;
    }
    return true;
}

static void addFontFiles(const Vector<String>& fontFiles, FcConfig* config)
{
    for (Vector<String>::const_iterator it = fontFiles.begin(); it != fontFiles.end(); ++it) {
        const CString currentFile = (*it).utf8();
        const char* path = currentFile.data();

        if (!FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(path)))
            fprintf(stderr, "Could not load font at %s!\n", path);
    }
}

void addFontsToEnvironment()
{
    FcInit();

    // Load our configuration file, which sets up proper aliases for family
    // names like sans, serif and monospace.
    FcConfig* config = FcConfigCreate();
    const char* fontConfigFilename = FONTS_CONF_DIR "/fonts.conf";
    if (!FcConfigParseAndLoad(config, reinterpret_cast<const FcChar8*>(fontConfigFilename), true)) {
        fprintf(stderr, "Couldn't load font configuration file from: %s\n", fontConfigFilename);
        exit(1);
    }

    if (!addFontDirectory(DOWNLOADED_FONTS_DIR, config)) {
        fprintf(stderr, "None of the font directories could be added. Either install them "
                        "or file a bug at http://bugs.webkit.org if they are installed in "
                        "another location.\n");
        exit(1);
    }

    addFontFiles(getFontFiles(), config);

    if (!FcConfigSetCurrent(config)) {
        fprintf(stderr, "Could not set the current font configuration!\n");
        exit(1);
    }
}

