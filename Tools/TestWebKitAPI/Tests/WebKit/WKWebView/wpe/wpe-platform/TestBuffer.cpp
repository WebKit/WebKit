/*
 * Copyright (C) 2026 Igalia, S.L.
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

#include "WPEMockPlatformTest.h"
#include <wtf/glib/GUniquePtr.h>

namespace TestWebKitAPI {

static void testBufferImportToPixelsPaddedRows(WPEMockPlatformTest* test, gconstpointer)
{
    // Rows padded to 512 bytes, more than the 400 bytes of pixels.
    const int width = 100;
    const int height = 4;
    const guint stride = 512;
    const guint rowSize = width * 4;
    auto* data = static_cast<guint8*>(g_malloc(stride * height));
    for (guint y = 0; y < height; ++y) {
        for (guint x = 0; x < stride; ++x)
            data[y * stride + x] = x < rowSize ? y + 1 : 0xff;
    }
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_take(data, stride * height));
    GRefPtr<WPEBuffer> buffer = adoptGRef(WPE_BUFFER(wpe_buffer_shm_new(test->display(), width, height, WPE_PIXEL_FORMAT_ARGB8888, bytes.get(), stride)));
    test->assertObjectIsDeletedWhenTestFinishes(buffer.get());

    GUniqueOutPtr<GError> error;
    auto* pixels = wpe_buffer_import_to_pixels(buffer.get(), &error.outPtr());
    g_assert_no_error(error.get());
    g_assert_nonnull(pixels);
    gsize size;
    const auto* pixelsData = static_cast<const guint8*>(g_bytes_get_data(pixels, &size));
    g_assert_cmpuint(size, ==, rowSize * height);
    for (guint y = 0; y < height; ++y) {
        for (guint x = 0; x < rowSize; ++x)
            g_assert_cmpuint(pixelsData[y * rowSize + x], ==, y + 1);
    }
}

static void testBufferImportToPixelsUnpaddedRows(WPEMockPlatformTest* test, gconstpointer)
{
    const int width = 100;
    const int height = 4;
    const guint stride = width * 4;
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_take(g_malloc0(stride * height), stride * height));
    GRefPtr<WPEBuffer> buffer = adoptGRef(WPE_BUFFER(wpe_buffer_shm_new(test->display(), width, height, WPE_PIXEL_FORMAT_ARGB8888, bytes.get(), stride)));
    test->assertObjectIsDeletedWhenTestFinishes(buffer.get());

    // Rows without padding are returned without a copy.
    GUniqueOutPtr<GError> error;
    auto* pixels = wpe_buffer_import_to_pixels(buffer.get(), &error.outPtr());
    g_assert_no_error(error.get());
    g_assert_true(pixels == bytes.get());
}

void beforeAll()
{
    WPEMockPlatformTest::add("Buffer", "import-to-pixels-padded-rows", testBufferImportToPixelsPaddedRows);
    WPEMockPlatformTest::add("Buffer", "import-to-pixels-unpadded-rows", testBufferImportToPixelsUnpaddedRows);
}

void afterAll()
{
}

} // namespace TestWebKitAPI
