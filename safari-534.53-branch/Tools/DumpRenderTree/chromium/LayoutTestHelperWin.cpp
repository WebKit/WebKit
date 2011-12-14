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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

static BOOL fontSmoothingEnabled = FALSE;

static void saveInitialSettings(void)
{
    ::SystemParametersInfo(SPI_GETFONTSMOOTHING, 0, &fontSmoothingEnabled, 0);
}

// Technically, all we need to do is disable ClearType. However,
// for some reason, the call to SPI_SETFONTSMOOTHINGTYPE doesn't
// seem to work, so we just disable font smoothing all together
// (which works reliably)
static void installLayoutTestSettings(void)
{
    ::SystemParametersInfo(SPI_SETFONTSMOOTHING, FALSE, 0, 0);
}

static void restoreInitialSettings(void)
{
    ::SystemParametersInfo(SPI_SETFONTSMOOTHING, static_cast<UINT>(fontSmoothingEnabled), 0, 0);
}

static void simpleSignalHandler(int signalNumber)
{
    // Try to restore the settings and then go down cleanly
    restoreInitialSettings();
    exit(128 + signalNumber);
}

int main(int, char*[])
{
    // Hooks the ways we might get told to clean up...
    signal(SIGINT, simpleSignalHandler);
    signal(SIGTERM, simpleSignalHandler);

    saveInitialSettings();

    installLayoutTestSettings();

    // Let the script know we're ready
    printf("ready\n");
    fflush(stdout);

    // Wait for any key (or signal)
    getchar();

    restoreInitialSettings();

    return EXIT_SUCCESS;
}
