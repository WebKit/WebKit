/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "TestMain.h"
#include <WebKitImagePrivate.h>
#include <span>
#include <wtf/glib/GUniquePtr.h>

class WebKitImageTest : public Test {
public:
    MAKE_GLIB_TEST_FIXTURE(WebKitImageTest);

    static void asyncLoadFinishedCallback(GObject* source, GAsyncResult* result, gpointer userData)
    {
        auto* test = static_cast<WebKitImageTest*>(userData);
        GUniqueOutPtr<char> type;
        GUniqueOutPtr<GError> error;
        GInputStream* stream = g_loadable_icon_load_finish(G_LOADABLE_ICON(source),
        result, &type.outPtr(), &error.outPtr());

        g_assert_no_error(error.get());
        g_assert_nonnull(stream);
        g_assert_cmpstr(type.get(), ==, "image/png");

        gsize bytes_read;
        unsigned char buffer[8];
        gboolean success = g_input_stream_read_all(stream, buffer, sizeof(buffer),
            &bytes_read, nullptr, &error.outPtr());

        g_assert_no_error(error.get());
        g_assert_true(success);
        g_assert_cmpuint(bytes_read, ==, 8);
        g_assert_cmpint(buffer[0], ==, 0x89);
        g_assert_cmpint(buffer[1], ==, 'P');
        g_assert_cmpint(buffer[2], ==, 'N');
        g_assert_cmpint(buffer[3], ==, 'G');
        g_assert_cmpint(buffer[4], ==, 0x0D);
        g_assert_cmpint(buffer[5], ==, 0x0A);
        g_assert_cmpint(buffer[6], ==, 0x1A);
        g_assert_cmpint(buffer[7], ==, 0x0A);

        g_main_loop_quit(test->m_mainLoop);
    }

    WebKitImageTest()
        : m_mainLoop(g_main_loop_new(nullptr, TRUE))
    {

    }
    ~WebKitImageTest()
    {
        g_main_loop_unref(m_mainLoop);
    }

    GMainLoop* m_mainLoop;
};

static void testWebKitImagePropertiesConstruct(WebKitImageTest*, gconstpointer)
{
    GRefPtr<WebKitImage> image = adoptGRef(webkitImageNew(1, 2, 4, adoptGRef(g_bytes_new_static("test_data", 9))));

    int width, height;
    GUniqueOutPtr<char> type;
    guint stride;

    g_object_get(image.get(),
        "width", &width,
        "height", &height,
        "stride", &stride,
        nullptr);

    GBytes* retrieved_data = webkit_image_as_bytes(image.get());

    g_assert_cmpint(width, ==, 1);
    g_assert_cmpint(height, ==, 2);
    g_assert_cmpuint(stride, ==, 4);
    GRefPtr<GBytes> data = adoptGRef(g_bytes_new_static("test_data", 9));
    g_assert_true(g_bytes_equal(data.get(), retrieved_data));
}

static void testWebKitImageIconInterface(WebKitImageTest*, gconstpointer)
{
    auto makeRGBAData = [](int width, int height, int stride, uint8_t fillValue) -> GRefPtr<GBytes> {
        gsize size = stride * height;
        auto* data = static_cast<uint8_t*>(g_malloc(size));
        auto span = std::span<uint8_t>(data, size);
        memsetSpan(span, fillValue);
        return adoptGRef(g_bytes_new_take(data, size));
    };

    GRefPtr<WebKitImage> image1 = adoptGRef(webkitImageNew(2, 2, 8, makeRGBAData(2, 2, 8, 0xAA)));
    GRefPtr<WebKitImage> image2 = adoptGRef(webkitImageNew(2, 2, 8, makeRGBAData(2, 2, 8, 0xAA)));

    auto* icon1 = G_ICON(image1.get());
    auto* icon2 = G_ICON(image2.get());
    g_assert_true(g_icon_equal(icon1, icon2));

    guint hash1 = g_icon_hash(icon1);
    guint hash2 = g_icon_hash(icon2);
    g_assert_cmpuint(hash1, ==, hash2);

    GRefPtr<WebKitImage> image3 = adoptGRef(webkitImageNew(2, 2, 12, makeRGBAData(2, 2, 12, 0xAA)));

    auto* icon3 = G_ICON(image3.get());
    g_assert_true(g_icon_equal(icon1, icon3));

    guint hash3 = g_icon_hash(icon3);
    g_assert_cmpuint(hash1, ==, hash3);

    GRefPtr<WebKitImage> image4 = adoptGRef(webkitImageNew(2, 2, 8, makeRGBAData(2, 2, 8, 0xBB)));

    auto* icon4 = G_ICON(image4.get());
    g_assert_false(g_icon_equal(icon1, icon4));

    guint hash4 = g_icon_hash(icon4);
    g_assert_cmpuint(hash1, !=, hash4);

    GRefPtr<WebKitImage> image5 = adoptGRef(webkitImageNew(3, 3, 16, makeRGBAData(3, 3, 16, 0xCC)));

    auto* icon5 = G_ICON(image5.get());
    g_assert_false(g_icon_equal(icon1, icon5));

    guint hash5 = g_icon_hash(icon5);
    g_assert_cmpuint(hash1, !=, hash5);
}

static void testWebKitImageLoadableIconLoadSync(WebKitImageTest*, gconstpointer)
{
    auto makeSingleRedPixel = []() -> GRefPtr<GBytes> {
        uint8_t pixel[] = { 255, 0, 0, 255 };
        return adoptGRef(g_bytes_new(pixel, sizeof(pixel)));
    };
    GRefPtr<WebKitImage> image = webkitImageNew(1, 1, 4, makeSingleRedPixel());
    auto* icon = G_LOADABLE_ICON(image.get());

    GUniqueOutPtr<GError> error;
    GUniqueOutPtr<char> type;

    GRefPtr<GInputStream> stream = adoptGRef(g_loadable_icon_load(icon, 0, &type.outPtr(), nullptr, &error.outPtr()));

    g_assert_no_error(error.get());
    g_assert_nonnull(stream);
    g_assert_cmpstr(type.get(), ==, "image/png");

    gsize bytes_read;
    unsigned char buffer[8];
    gboolean success = g_input_stream_read_all(stream.get(), buffer, sizeof(buffer),
        &bytes_read, nullptr, &error.outPtr());

    g_assert_no_error(error.get());
    g_assert_true(success);
    g_assert_cmpuint(bytes_read, ==, 8);
    g_assert_cmpint(buffer[0], ==, 0x89);
    g_assert_cmpint(buffer[1], ==, 'P');
    g_assert_cmpint(buffer[2], ==, 'N');
    g_assert_cmpint(buffer[3], ==, 'G');
    g_assert_cmpint(buffer[4], ==, 0x0D);
    g_assert_cmpint(buffer[5], ==, 0x0A);
    g_assert_cmpint(buffer[6], ==, 0x1A);
    g_assert_cmpint(buffer[7], ==, 0x0A);
}

static void testWebKitImageLoadableIconLoadAsync(WebKitImageTest* test, gconstpointer)
{
    auto makeSingleRedPixel = []() -> GRefPtr<GBytes> {
        uint8_t pixel[] = { 255, 0, 0, 255 };
        return adoptGRef(g_bytes_new(pixel, sizeof(pixel)));
    };
    GRefPtr<WebKitImage> image = webkitImageNew(1, 1, 4, makeSingleRedPixel());
    auto* icon = G_LOADABLE_ICON(image.get());

    g_loadable_icon_load_async(icon, 0, nullptr, WebKitImageTest::asyncLoadFinishedCallback, test);

    g_main_loop_run(test->m_mainLoop);
}

void beforeAll()
{
    WebKitImageTest::add("WebKitImage", "create-and-get", testWebKitImagePropertiesConstruct);
    WebKitImageTest::add("WebKitImage", "icon-interface", testWebKitImageIconInterface);
    WebKitImageTest::add("WebKitImage", "loadable-icon-interface-sync-load", testWebKitImageLoadableIconLoadSync);
    WebKitImageTest::add("WebKitImage", "loadable-icon-interface-async-load", testWebKitImageLoadableIconLoadAsync);
}

void afterAll()
{
}
