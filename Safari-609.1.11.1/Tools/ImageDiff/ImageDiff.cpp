/*
 * Copyright (C) 2017 Igalia S.L.
 * Copyright (C) 2005, 2007, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2005 Ben La Monica <ben.lamonica@gmail.com>.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// FIXME: We need to be able to include these defines from a config.h somewhere.
#define JS_EXPORT_PRIVATE

#include "PlatformImage.h"
#include <algorithm>
#include <cstdlib>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#endif

using namespace ImageDiff;

#ifdef _WIN32
#define FORMAT_SIZE_T "Iu"
#else
#define FORMAT_SIZE_T "zu"
#endif

int main(int argc, const char* argv[])
{
#ifdef _WIN32
    _setmode(0, _O_BINARY);
    _setmode(1, _O_BINARY);
#endif

    float tolerance = 0.0f;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--tolerance")) {
            if (i >= argc - 1)
                exit(1);
            tolerance = strtof(argv[i + 1], 0);
            ++i;
            continue;
        }
    }

    char buffer[2048];
    std::unique_ptr<PlatformImage> actualImage;
    std::unique_ptr<PlatformImage> baselineImage;

    while (fgets(buffer, sizeof(buffer), stdin)) {
        // Convert the first newline into a NUL character so that strtok doesn't produce it.
        char* newLineCharacter = strchr(buffer, '\n');
        if (newLineCharacter)
            *newLineCharacter = '\0';

        if (!strncmp("Content-Length: ", buffer, 16)) {
            strtok(buffer, " ");

            int imageSize = strtol(strtok(0, " "), 0, 10);
            if (imageSize <= 0) {
                fprintf(stderr, "Error: image size must be specified.\n");
                return EXIT_FAILURE;
            }

            if (!actualImage) {
                if (!(actualImage = PlatformImage::createFromStdin(imageSize))) {
                    fprintf(stderr, "Error: could not read actual image.\n");
                    return EXIT_FAILURE;
                }
            } else if (!baselineImage) {
                if (!(baselineImage = PlatformImage::createFromStdin(imageSize))) {
                    fprintf(stderr, "Error: could not read baseline image.\n");
                    return EXIT_FAILURE;
                }
            }
        }

        if (actualImage && baselineImage) {
            if (!actualImage->isCompatible(*baselineImage)) {
                if (actualImage->width() != baselineImage->width() || actualImage->height() != baselineImage->height()) {
                    fprintf(stderr, "Error: test and reference images have different sizes. Test image is %" FORMAT_SIZE_T "x%" FORMAT_SIZE_T ", reference image is %" FORMAT_SIZE_T "x%" FORMAT_SIZE_T "\n",
                        actualImage->width(), actualImage->height(), baselineImage->width(), baselineImage->height());
                } else if (actualImage->hasAlpha() != baselineImage->hasAlpha()) {
                    fprintf(stderr, "Error: test and reference images differ in alpha. Test image %s alpha, reference image %s alpha.\n",
                        actualImage->hasAlpha() ? "has" : "does not have", baselineImage->hasAlpha() ? "has" : "does not have");
                }

                return EXIT_FAILURE;
            }

            float difference = 100.0f;
            std::unique_ptr<PlatformImage> diffImage = actualImage->difference(*baselineImage, difference);
            if (difference <= tolerance)
                difference = 0.0f;
            else {
                difference = roundf(difference * 100.0f) / 100.0f;
                difference = std::max<float>(difference, 0.01f); // round to 2 decimal places
            }

            if (difference > 0.0f) {
                if (diffImage)
                    diffImage->writeAsPNGToStdout();
                fprintf(stdout, "diff: %01.2f%% failed\n", difference);
            } else
                fprintf(stdout, "diff: %01.2f%% passed\n", difference);

            actualImage = nullptr;
            baselineImage = nullptr;
        }

        fflush(stdout);
    }

    return EXIT_SUCCESS;
}

#ifdef _WIN32
extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[])
{
    return main(argc, argv);
}
#endif
