/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

static int processImages(std::unique_ptr<PlatformImage>&& actualImage, std::unique_ptr<PlatformImage>&& baselineImage, bool exact, bool printDifference)
{
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

    PlatformImage::Difference differenceData = { 100, 0, 0 };
    auto diffImage = actualImage->difference(*baselineImage, exact, differenceData);
    if (diffImage)
        diffImage->writeAsPNGToStdout();

    fprintf(stdout, "diff: %01.8f%%\n", differenceData.percentageDifference);

    if (printDifference)
        fprintf(stdout, "maxDifference=%u; totalPixels=%zu\n", differenceData.maxDifference, differenceData.totalPixels);

    fprintf(stdout, "#EOF\n");
    fflush(stdout);

    return EXIT_SUCCESS;
}

int main(int argc, const char* argv[])
{
#ifdef _WIN32
    _setmode(0, _O_BINARY);
    _setmode(1, _O_BINARY);
#endif

    bool verbose = false;
    bool exact = false;
    bool printDifference = false;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
            verbose = true;
            continue;
        }

        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--difference")) {
            printDifference = true;
            continue;
        }

        if (!strcmp(argv[i], "-e") || !strcmp(argv[i], "--exact")) {
            exact = true;
            continue;
        }

        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            fprintf(stdout,
                "usage: ImageDiff [-h] [-v] [-d] [-e] ([actualImage baselineImage] | <stdin>)\n" \
                "\n" \
                "Reads two PNG-encoded images and compares them. If two file path arguments are supplied, \n" \
                "reads from the specified files, otherwise from <stdin> where each file is preceded by \n" \
                "a \"Content-Length:\" header.\n" \
                "\n" \
                "optional arguments:\n" \
                "  -h, --help            show this help message and exit\n" \
                "  -v, --verbose         print diagnostic information to stderr\n" \
                "  -d, --difference      print WPT-style maxDifference and totalPixels data\n" \
                "  -e, --exact           use exact matching (no built-in per-component tolerance)\n"
            );
            return EXIT_SUCCESS;
        }

        if (i < argc - 1) {
            const char* file1Path = argv[i];
            const char* file2Path = argv[i + 1];

            if (file1Path[0] ==  '-') {
                fprintf(stderr, "Ambiguous file path argument %s\n", file1Path);
                return EXIT_FAILURE;
            }

            if (file2Path[0] ==  '-') {
                fprintf(stderr, "Ambiguous file path argument %s\n", file2Path);
                return EXIT_FAILURE;
            }

            auto actualImage = PlatformImage::createFromFile(file1Path);
            auto baselineImage = PlatformImage::createFromFile(file2Path);

            if (!actualImage) {
                fprintf(stderr, "Failed to create image from %s\n", file1Path);
                return EXIT_FAILURE;
            }

            if (!baselineImage) {
                fprintf(stderr, "Failed to create image from %s\n", file2Path);
                return EXIT_FAILURE;
            }
            
            if (verbose)
                fprintf(stderr, "Comparing files actual: %s and baseline: %s\n", file1Path, file2Path);

            return processImages(std::move(actualImage), std::move(baselineImage), exact, printDifference);
        }
    }

    char buffer[2048];
    std::unique_ptr<PlatformImage> actualImage;
    std::unique_ptr<PlatformImage> baselineImage;

    while (fgets(buffer, sizeof(buffer), stdin)) {
        if (verbose)
            fprintf(stderr, "ImageDiff: read %" FORMAT_SIZE_T " bytes from stdin %s", strlen(buffer), buffer);

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
                if (verbose)
                    fprintf(stderr, "Reading %d bytes of actual image data\n", imageSize);

                if (!(actualImage = PlatformImage::createFromStdin(imageSize))) {
                    fprintf(stderr, "Error: could not read actual image.\n");
                    return EXIT_FAILURE;
                }
            } else if (!baselineImage) {
                if (verbose)
                    fprintf(stderr, "Reading %d bytes of baseline image data\n", imageSize);

                if (!(baselineImage = PlatformImage::createFromStdin(imageSize))) {
                    fprintf(stderr, "Error: could not read baseline image.\n");
                    return EXIT_FAILURE;
                }
            }
        }

        if (actualImage && baselineImage) {
            auto result = processImages(std::exchange(actualImage, { }), std::exchange(baselineImage, { }), exact, printDifference);
            if (result != EXIT_SUCCESS)
                return result;
        }
    }

    return EXIT_SUCCESS;
}

#ifdef _WIN32
extern "C" __declspec(dllexport) int WINAPI dllLauncherEntryPoint(int argc, const char* argv[])
{
    return main(argc, argv);
}
#endif
