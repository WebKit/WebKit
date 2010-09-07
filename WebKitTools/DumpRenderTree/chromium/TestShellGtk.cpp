/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestShell.h"

#include "webkit/support/webkit_support.h"
#include <fontconfig/fontconfig.h>
#include <gtk/gtk.h>
#include <signal.h>

static void AlarmHandler(int signatl)
{
    // If the alarm alarmed, kill the process since we have a really bad hang.
    puts("\n#TEST_TIMED_OUT\n");
    puts("#EOF\n");
    fflush(stdout);
    exit(0);
}

static void setupFontconfig()
{
    // We wish to make the layout tests reproducable with respect to fonts. Skia
    // uses fontconfig to resolve font family names from WebKit into actual font
    // files found on the current system. This means that fonts vary based on the
    // system and also on the fontconfig configuration.
    //
    // To avoid this we initialise fontconfig here and install a configuration
    // which only knows about a few, select, fonts.

    // We have fontconfig parse a config file from our resources file. This
    // sets a number of aliases ("sans"->"Arial" etc), but doesn't include any
    // font directories.
    FcInit();

    char drtPath[PATH_MAX + 1];
    int drtPathSize = readlink("/proc/self/exe", drtPath, PATH_MAX);
    if (drtPathSize < 0 || drtPathSize > PATH_MAX) {
        fputs("Unable to resolve /proc/self/exe.", stderr);
        exit(1);
    }
    drtPath[drtPathSize] = 0;
    std::string drtDirPath(drtPath);
    size_t lastPathPos = drtDirPath.rfind("/");
    ASSERT(lastPathPos != std::string::npos);
    drtDirPath.erase(lastPathPos + 1);

    FcConfig* fontcfg = FcConfigCreate();
    std::string fontconfigPath = drtDirPath + "fonts.conf";
    if (!FcConfigParseAndLoad(fontcfg, reinterpret_cast<const FcChar8*>(fontconfigPath.c_str()), true)) {
        fputs("Failed to parse fontconfig config file\n", stderr);
        exit(1);
    }

    // This is the list of fonts that fontconfig will know about. It
    // will try its best to match based only on the fonts here in. The
    // paths are where these fonts are found on our Ubuntu boxes.
    static const char *const fonts[] = {
        "/usr/share/fonts/truetype/kochi/kochi-gothic.ttf",
        "/usr/share/fonts/truetype/kochi/kochi-mincho.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Arial_Bold_Italic.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Arial_Italic.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Comic_Sans_MS_Bold.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Courier_New.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Bold_Italic.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Courier_New_Italic.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Georgia.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Georgia_Bold.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Georgia_Bold_Italic.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Georgia_Italic.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Impact.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS_Bold.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS_Bold_Italic.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Trebuchet_MS_Italic.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Bold_Italic.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Times_New_Roman_Italic.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Verdana.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Verdana_Bold.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Verdana_Bold_Italic.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Verdana_Italic.ttf",
        // The DejaVuSans font is used by the css2.1 tests.
        "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/ttf-indic-fonts-core/lohit_ta.ttf",
        "/usr/share/fonts/truetype/ttf-indic-fonts-core/MuktiNarrow.ttf",
    };
    for (size_t i = 0; i < arraysize(fonts); ++i) {
        if (access(fonts[i], R_OK)) {
            fprintf(stderr, "You are missing %s. Try installing msttcorefonts. Also see "
                            "http://code.google.com/p/chromium/wiki/LinuxBuildInstructions",
                            fonts[i]);
            exit(1);
        }
        if (!FcConfigAppFontAddFile(fontcfg, (FcChar8 *) fonts[i])) {
            fprintf(stderr, "Failed to load font %s\n", fonts[i]);
            exit(1);
        }
    }

    // We special case these fonts because they're only needed in a
    // few layout tests.
    static const char* const optionalFonts[] = {
        "/usr/share/fonts/truetype/ttf-lucida/LucidaSansRegular.ttf",

        // This font changed paths across Ubuntu releases.
        "/usr/share/fonts/truetype/ttf-indic-fonts-core/lohit_pa.ttf",
        "/usr/share/fonts/truetype/ttf-punjabi-fonts/lohit_pa.ttf",
    };
    for (size_t i = 0; i < arraysize(optionalFonts); ++i) {
        const char* font = optionalFonts[i];
        if (access(font, R_OK) < 0) {
            fprintf(stderr, "You are missing %s. Without this, some layout tests may fail. "
                            "See http://code.google.com/p/chromium/wiki/LinuxBuildInstructionsPrerequisites "
                            "for more.\n", font);
        } else {
            if (!FcConfigAppFontAddFile(fontcfg, (FcChar8 *) font)) {
                fprintf(stderr, "Failed to load font %s\n", font);
                exit(1);
            }
        }
    }

    // Also load the layout-test-specific "Ahem" font.
    std::string ahemPath = drtDirPath + "AHEM____.TTF";
    if (!FcConfigAppFontAddFile(fontcfg, reinterpret_cast<const FcChar8*>(ahemPath.c_str()))) {
        fprintf(stderr, "Failed to load font %s\n", ahemPath.c_str());
        exit(1);
    }

    if (!FcConfigSetCurrent(fontcfg)) {
        fputs("Failed to set the default font configuration\n", stderr);
        exit(1);
    }
}

void TestShell::waitTestFinished()
{
    ASSERT(!m_testIsPending);

    m_testIsPending = true;

    // Install an alarm signal handler that will kill us if we time out.
    signal(SIGALRM, AlarmHandler);
    alarm(layoutTestTimeoutForWatchDog() / 1000);

    // TestFinished() will post a quit message to break this loop when the page
    // finishes loading.
    while (m_testIsPending)
        webkit_support::RunMessageLoop();

    // Remove the alarm.
    alarm(0);
    signal(SIGALRM, SIG_DFL);
}

void platformInit(int* argc, char*** argv)
{
    // FIXME: It's better call gtk_init() only when we run plugin tests.
    // See http://groups.google.com/a/chromium.org/group/chromium-dev/browse_thread/thread/633ea167cde196ca#
    gtk_init(argc, argv);

    setupFontconfig();
}

void openStartupDialog()
{
    GtkWidget* dialog = gtk_message_dialog_new(
        0, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "Attach to me?");
    gtk_window_set_title(GTK_WINDOW(dialog), "DumpRenderTree");
    gtk_dialog_run(GTK_DIALOG(dialog)); // Runs a nested message loop.
    gtk_widget_destroy(dialog);
}

bool checkLayoutTestSystemDependencies()
{
    return true;
}
