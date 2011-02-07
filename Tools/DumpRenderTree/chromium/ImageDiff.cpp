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

// This file input format is based loosely on
// WebKitTools/DumpRenderTree/ImageDiff.m

// The exact format of this tool's output to stdout is important, to match
// what the run-webkit-tests script expects.

#include "config.h"

#include "webkit/support/webkit_support_gfx.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <wtf/OwnArrayPtr.h>
#include <wtf/Vector.h>

#if OS(WINDOWS)
#include <windows.h>
#define PATH_MAX MAX_PATH
#endif

using namespace std;

// Causes the app to remain open, waiting for pairs of filenames on stdin.
// The caller is then responsible for terminating this app.
static const char optionPollStdin[] = "--use-stdin";
static const char optionGenerateDiff[] = "--diff";

// Return codes used by this utility.
static const int statusSame = 0;
static const int statusDifferent = 1;
static const int statusError = 2;

// Color codes.
static const uint32_t rgbaRed = 0x000000ff;
static const uint32_t rgbaAlpha = 0xff000000;

class Image {
public:
    Image()
        : m_width(0)
        , m_height(0) {}

    Image(const Image& image)
        : m_width(image.m_width)
        , m_height(image.m_height)
        , m_data(image.m_data) {}

    bool hasImage() const { return m_width > 0 && m_height > 0; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    const unsigned char* data() const { return &m_data.front(); }

    // Creates the image from stdin with the given data length. On success, it
    // will return true. On failure, no other methods should be accessed.
    bool craeteFromStdin(size_t byteLength)
    {
        if (!byteLength)
            return false;

        OwnArrayPtr<unsigned char> source = adoptArrayPtr(new unsigned char[byteLength]);
        if (fread(source.get(), 1, byteLength, stdin) != byteLength)
            return false;

        if (!webkit_support::DecodePNG(source.get(), byteLength, &m_data, &m_width, &m_height)) {
            clear();
            return false;
        }
        return true;
    }

    // Creates the image from the given filename on disk, and returns true on
    // success.
    bool createFromFilename(const char* filename)
    {
        FILE* f = fopen(filename, "rb");
        if (!f)
            return false;

        vector<unsigned char> compressed;
        const int bufSize = 1024;
        unsigned char buf[bufSize];
        size_t numRead = 0;
        while ((numRead = fread(buf, 1, bufSize, f)) > 0)
            std::copy(buf, &buf[numRead], std::back_inserter(compressed));

        fclose(f);

        if (!webkit_support::DecodePNG(&compressed[0], compressed.size(), &m_data, &m_width, &m_height)) {
            clear();
            return false;
        }
        return true;
    }

    void clear()
    {
        m_width = m_height = 0;
        m_data.clear();
    }

    // Returns the RGBA value of the pixel at the given location
    const uint32_t pixelAt(int x, int y) const
    {
        ASSERT(x >= 0 && x < m_width);
        ASSERT(y >= 0 && y < m_height);
        return *reinterpret_cast<const uint32_t*>(&(m_data[(y * m_width + x) * 4]));
    }

    void setPixelAt(int x, int y, uint32_t color) const
    {
        ASSERT(x >= 0 && x < m_width);
        ASSERT(y >= 0 && y < m_height);
        void* addr = &const_cast<unsigned char*>(&m_data.front())[(y * m_width + x) * 4];
        *reinterpret_cast<uint32_t*>(addr) = color;
    }

private:
    // pixel dimensions of the image
    int m_width, m_height;

    vector<unsigned char> m_data;
};

float percentageDifferent(const Image& baseline, const Image& actual)
{
    int w = min(baseline.width(), actual.width());
    int h = min(baseline.height(), actual.height());

    // Compute pixels different in the overlap
    int pixelsDifferent = 0;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            if (baseline.pixelAt(x, y) != actual.pixelAt(x, y))
                pixelsDifferent++;
        }
    }

    // Count pixels that are a difference in size as also being different
    int maxWidth = max(baseline.width(), actual.width());
    int maxHeight = max(baseline.height(), actual.height());

    // ...pixels off the right side, but not including the lower right corner
    pixelsDifferent += (maxWidth - w) * h;

    // ...pixels along the bottom, including the lower right corner
    pixelsDifferent += (maxHeight - h) * maxWidth;

    // Like the WebKit ImageDiff tool, we define percentage different in terms
    // of the size of the 'actual' bitmap.
    float totalPixels = static_cast<float>(actual.width()) * static_cast<float>(actual.height());
    if (!totalPixels)
        return 100.0f; // When the bitmap is empty, they are 100% different.
    return static_cast<float>(pixelsDifferent) / totalPixels * 100;
}

void printHelp()
{
    fprintf(stderr,
            "Usage:\n"
            "  ImageDiff <compare file> <reference file>\n"
            "    Compares two files on disk, returning 0 when they are the same\n"
            "  ImageDiff --use-stdin\n"
            "    Stays open reading pairs of filenames from stdin, comparing them,\n"
            "    and sending 0 to stdout when they are the same\n"
            "  ImageDiff --diff <compare file> <reference file> <output file>\n"
            "    Compares two files on disk, outputs an image that visualizes the"
            "    difference to <output file>\n");
    /* For unfinished webkit-like-mode (see below)
       "\n"
       "  ImageDiff -s\n"
       "    Reads stream input from stdin, should be EXACTLY of the format\n"
       "    \"Content-length: <byte length> <data>Content-length: ...\n"
       "    it will take as many file pairs as given, and will compare them as\n"
       "    (cmp_file, reference_file) pairs\n");
    */
}

int compareImages(const char* file1, const char* file2)
{
    Image actualImage;
    Image baselineImage;

    if (!actualImage.createFromFilename(file1)) {
        fprintf(stderr, "ImageDiff: Unable to open file \"%s\"\n", file1);
        return statusError;
    }
    if (!baselineImage.createFromFilename(file2)) {
        fprintf(stderr, "ImageDiff: Unable to open file \"%s\"\n", file2);
        return statusError;
    }

    float percent = percentageDifferent(actualImage, baselineImage);
    if (percent > 0.0) {
        // failure: The WebKit version also writes the difference image to
        // stdout, which seems excessive for our needs.
        printf("diff: %01.2f%% failed\n", percent);
        return statusDifferent;
    }

    // success
    printf("diff: %01.2f%% passed\n", percent);
    return statusSame;

}

// Untested mode that acts like WebKit's image comparator. I wrote this but
// decided it's too complicated. We may use it in the future if it looks useful.
int untestedCompareImages()
{
    Image actualImage;
    Image baselineImage;
    char buffer[2048];
    while (fgets(buffer, sizeof(buffer), stdin)) {
        if (!strncmp("Content-length: ", buffer, 16)) {
            char* context;
#if OS(WINDOWS)
            strtok_s(buffer, " ", &context);
            int imageSize = strtol(strtok_s(0, " ", &context), 0, 10);
#else
            strtok_r(buffer, " ", &context);
            int imageSize = strtol(strtok_r(0, " ", &context), 0, 10);
#endif

            bool success = false;
            if (imageSize > 0 && !actualImage.hasImage()) {
                if (!actualImage.craeteFromStdin(imageSize)) {
                    fputs("Error, input image can't be decoded.\n", stderr);
                    return 1;
                }
            } else if (imageSize > 0 && !baselineImage.hasImage()) {
                if (!baselineImage.craeteFromStdin(imageSize)) {
                    fputs("Error, baseline image can't be decoded.\n", stderr);
                    return 1;
                }
            } else {
                fputs("Error, image size must be specified.\n", stderr);
                return 1;
            }
        }

        if (actualImage.hasImage() && baselineImage.hasImage()) {
            float percent = percentageDifferent(actualImage, baselineImage);
            if (percent > 0.0) {
                // failure: The WebKit version also writes the difference image to
                // stdout, which seems excessive for our needs.
                printf("diff: %01.2f%% failed\n", percent);
            } else {
                // success
                printf("diff: %01.2f%% passed\n", percent);
            }
            actualImage.clear();
            baselineImage.clear();
        }
        fflush(stdout);
    }
    return 0;
}

bool createImageDiff(const Image& image1, const Image& image2, Image* out)
{
    int w = min(image1.width(), image2.width());
    int h = min(image1.height(), image2.height());
    *out = Image(image1);
    bool same = (image1.width() == image2.width()) && (image1.height() == image2.height());

    // FIXME: do something with the extra pixels if the image sizes are different.
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint32_t basePixel = image1.pixelAt(x, y);
            if (basePixel != image2.pixelAt(x, y)) {
                // Set differing pixels red.
                out->setPixelAt(x, y, rgbaRed | rgbaAlpha);
                same = false;
            } else {
                // Set same pixels as faded.
                uint32_t alpha = basePixel & rgbaAlpha;
                uint32_t newPixel = basePixel - ((alpha / 2) & rgbaAlpha);
                out->setPixelAt(x, y, newPixel);
            }
        }
    }

    return same;
}

static bool writeFile(const char* outFile, const unsigned char* data, size_t dataSize)
{
    FILE* file = fopen(outFile, "wb");
    if (!file) {
        fprintf(stderr, "ImageDiff: Unable to create file \"%s\"\n", outFile);
        return false;
    }
    if (dataSize != fwrite(data, 1, dataSize, file)) {
        fclose(file);
        fprintf(stderr, "ImageDiff: Unable to write data to file \"%s\"\n", outFile);
        return false;
    }
    fclose(file);
    return true;
}

int diffImages(const char* file1, const char* file2, const char* outFile)
{
    Image actualImage;
    Image baselineImage;

    if (!actualImage.createFromFilename(file1)) {
        fprintf(stderr, "ImageDiff: Unable to open file \"%s\"\n", file1);
        return statusError;
    }
    if (!baselineImage.createFromFilename(file2)) {
        fprintf(stderr, "ImageDiff: Unable to open file \"%s\"\n", file2);
        return statusError;
    }

    Image diffImage;
    bool same = createImageDiff(baselineImage, actualImage, &diffImage);
    if (same)
        return statusSame;

    vector<unsigned char> pngData;
    webkit_support::EncodeRGBAPNG(diffImage.data(), diffImage.width(), diffImage.height(),
                                  diffImage.width() * 4, &pngData);
    if (!writeFile(outFile, &pngData.front(), pngData.size()))
        return statusError;
    return statusDifferent;
}

int main(int argc, const char* argv[])
{
    Vector<const char*> values;
    bool pollStdin = false;
    bool generateDiff = false;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], optionPollStdin))
            pollStdin = true;
        else if (!strcmp(argv[i], optionGenerateDiff))
            generateDiff = true;
        else
            values.append(argv[i]);
    }

    if (pollStdin) {
        // Watch stdin for filenames.
        const size_t bufferSize = PATH_MAX;
        char stdinBuffer[bufferSize];
        char firstName[bufferSize];
        bool haveFirstName = false;
        while (fgets(stdinBuffer, bufferSize, stdin)) {
            if (!stdinBuffer[0])
                continue;

            if (haveFirstName) {
                // compareImages writes results to stdout unless an error occurred.
                if (compareImages(firstName, stdinBuffer) == statusError)
                    printf("error\n");
                fflush(stdout);
                haveFirstName = false;
            } else {
                // Save the first filename in another buffer and wait for the second
                // filename to arrive via stdin.
                strcpy(firstName, stdinBuffer);
                haveFirstName = true;
            }
        }
        return 0;
    }

    if (generateDiff) {
        if (values.size() == 3)
            return diffImages(values[0], values[1], values[2]);
    } else if (values.size() == 2)
        return compareImages(argv[1], argv[2]);

    printHelp();
    return statusError;
}
