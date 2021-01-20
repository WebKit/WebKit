/*
 * Copyright (C) 2019 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#include "WebViewTest.h"
#include <wtf/WallTime.h>

class GeolocationTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(GeolocationTest);

    struct Position {
        double timestamp { std::numeric_limits<double>::quiet_NaN() };
        double latitude { std::numeric_limits<double>::quiet_NaN() };
        double longitude { std::numeric_limits<double>::quiet_NaN() };
        double accuracy { std::numeric_limits<double>::quiet_NaN() };

        Optional<double> altitude;
        Optional<double> altitudeAccuracy;
        Optional<double> heading;
        Optional<double> speed;
    };

    static gboolean startCallback(WebKitGeolocationManager* manager, GeolocationTest* test)
    {
        g_assert_true(test->m_manager == manager);
        test->start();
        return TRUE;
    }

    static void stopCallback(WebKitGeolocationManager* manager, GeolocationTest* test)
    {
        g_assert_true(test->m_manager == manager);
        test->stop();
    }

    static gboolean permissionRequested(WebKitWebView*, WebKitPermissionRequest* request, GeolocationTest* test)
    {
        g_assert_true(WEBKIT_IS_PERMISSION_REQUEST(request));
        test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(request));

        if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST(request)) {
            webkit_permission_request_allow(request);
            return TRUE;
        }

        return FALSE;
    }

    GeolocationTest()
        : m_manager(webkit_web_context_get_geolocation_manager(m_webContext.get()))
    {
        g_assert_true(WEBKIT_IS_GEOLOCATION_MANAGER(m_manager));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_manager));
        g_signal_connect(m_manager, "start", G_CALLBACK(startCallback), this);
        g_signal_connect(m_manager, "stop", G_CALLBACK(stopCallback), this);
        g_signal_connect(m_webView, "permission-request", G_CALLBACK(permissionRequested), this);
    }

    ~GeolocationTest()
    {
        if (m_checkPosition)
            webkit_geolocation_position_free(m_checkPosition);

        g_signal_handlers_disconnect_matched(m_manager, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
        g_signal_handlers_disconnect_matched(m_webView, G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, this);
    }

    void start()
    {
        g_assert_false(m_updating);
        m_updating = true;

        if (m_errorMessage) {
            g_assert_false(m_checkPosition);
            webkit_geolocation_manager_failed(m_manager, m_errorMessage.get());
        }

        if (m_checkPosition) {
            g_assert_false(m_errorMessage.get());
            webkit_geolocation_manager_update_position(m_manager, m_checkPosition);
        }

        g_main_loop_quit(m_mainLoop);
    }

    void stop()
    {
        g_assert_true(m_updating);
        m_updating = false;
        if (m_watching)
            g_main_loop_quit(m_mainLoop);
    }

    Position lastPosition()
    {
        GUniqueOutPtr<GError> error;
        auto* javascriptResult = runJavaScriptAndWaitUntilFinished("position", &error.outPtr());
        g_assert_nonnull(javascriptResult);
        g_assert_no_error(error.get());

        auto* jsPosition = webkit_javascript_result_get_js_value(javascriptResult);
        g_assert_true(jsc_value_is_object(jsPosition));
        Position result;
        GRefPtr<JSCValue> value = adoptGRef(jsc_value_object_get_property(jsPosition, "latitude"));
        g_assert_true(jsc_value_is_number(value.get()));
        result.latitude = jsc_value_to_double(value.get());
        value = adoptGRef(jsc_value_object_get_property(jsPosition, "longitude"));
        g_assert_true(jsc_value_is_number(value.get()));
        result.longitude = jsc_value_to_double(value.get());
        value = adoptGRef(jsc_value_object_get_property(jsPosition, "accuracy"));
        g_assert_true(jsc_value_is_number(value.get()));
        result.accuracy = jsc_value_to_double(value.get());
        value = adoptGRef(jsc_value_object_get_property(jsPosition, "altitude"));
        if (!jsc_value_is_null(value.get())) {
            g_assert_true(jsc_value_is_number(value.get()));
            result.altitude = jsc_value_to_double(value.get());
        }
        value = adoptGRef(jsc_value_object_get_property(jsPosition, "altitudeAccuracy"));
        if (!jsc_value_is_null(value.get())) {
            g_assert_true(jsc_value_is_number(value.get()));
            result.altitudeAccuracy = jsc_value_to_double(value.get());
        }
        value = adoptGRef(jsc_value_object_get_property(jsPosition, "heading"));
        if (!jsc_value_is_null(value.get())) {
            g_assert_true(jsc_value_is_number(value.get()));
            result.heading = jsc_value_to_double(value.get());
        }
        value = adoptGRef(jsc_value_object_get_property(jsPosition, "speed"));
        if (!jsc_value_is_null(value.get())) {
            g_assert_true(jsc_value_is_number(value.get()));
            result.speed = jsc_value_to_double(value.get());
        }
        value = adoptGRef(jsc_value_object_get_property(jsPosition, "timestamp"));
        g_assert_true(jsc_value_is_number(value.get()));
        result.timestamp = jsc_value_to_double(value.get());
        return result;
    }

    Position checkCurrentPosition(WebKitGeolocationPosition* position, bool enableHighAccuracy = false)
    {
        g_clear_pointer(&m_checkPosition, webkit_geolocation_position_free);
        m_checkPosition = position;

        static const char getCurrentPosition[] =
            "var position = null;\n"
            "options = { enableHighAccuracy: %s }\n"
            "navigator.geolocation.getCurrentPosition(function(p) {\n"
            "  position = { };\n"
            "  position.latitude = p.coords.latitude;\n"
            "  position.longitude = p.coords.longitude;\n"
            "  position.accuracy = p.coords.accuracy;\n"
            "  position.altitude = p.coords.altitude;\n"
            "  position.altitudeAccuracy = p.coords.altitudeAccuracy;\n"
            "  position.heading = p.coords.heading;\n"
            "  position.speed = p.coords.speed;\n"
            "  position.timestamp = p.timestamp;\n"
            "}, undefined, options);";
        GUniquePtr<char> script(g_strdup_printf(getCurrentPosition, enableHighAccuracy ? "true" : "false"));
        GUniqueOutPtr<GError> error;
        runJavaScriptAndWaitUntilFinished(script.get(), &error.outPtr());
        g_assert_no_error(error.get());
        g_main_loop_run(m_mainLoop);
        g_clear_pointer(&m_checkPosition, webkit_geolocation_position_free);
        g_assert_true(webkit_geolocation_manager_get_enable_high_accuracy(m_manager) == enableHighAccuracy);

        return lastPosition();
    }

    GUniquePtr<char> checkFailedToDeterminePosition(const char* errorMessage)
    {
        m_errorMessage.reset(g_strdup(errorMessage));
        static const char getCurrentPosition[] =
            "var error;\n"
            "navigator.geolocation.getCurrentPosition(\n"
            "  function(p) { error = null; },\n"
            "  function(e) { error = e.message.toString(); }\n"
            ");";
        GUniqueOutPtr<GError> error;
        auto* javascriptResult = runJavaScriptAndWaitUntilFinished(getCurrentPosition, &error.outPtr());
        g_assert_no_error(error.get());
        g_main_loop_run(m_mainLoop);
        m_errorMessage = nullptr;

        javascriptResult = runJavaScriptAndWaitUntilFinished("error", &error.outPtr());
        g_assert_nonnull(javascriptResult);
        g_assert_no_error(error.get());
        auto* jsErrorMessage = webkit_javascript_result_get_js_value(javascriptResult);
        g_assert_true(jsc_value_is_string(jsErrorMessage));
        return GUniquePtr<char>(jsc_value_to_string(jsErrorMessage));
    }

    void startWatch()
    {
        g_clear_pointer(&m_checkPosition, webkit_geolocation_position_free);
        m_errorMessage = nullptr;

        m_watching = true;
        static const char watchPosition[] =
            "var position = null;\n"
            "var watchID = navigator.geolocation.watchPosition(function(p) {\n"
            "  position = { };\n"
            "  position.latitude = p.coords.latitude;\n"
            "  position.longitude = p.coords.longitude;\n"
            "  position.accuracy = p.coords.accuracy;\n"
            "  position.altitude = p.coords.altitude;\n"
            "  position.altitudeAccuracy = p.coords.altitudeAccuracy;\n"
            "  position.heading = p.coords.heading;\n"
            "  position.speed = p.coords.speed;\n"
            "  position.timestamp = p.timestamp;\n"
            "});";
        GUniqueOutPtr<GError> error;
        auto* javascriptResult = runJavaScriptAndWaitUntilFinished(watchPosition, &error.outPtr());
        g_assert_nonnull(javascriptResult);
        g_assert_no_error(error.get());
        g_main_loop_run(m_mainLoop);
    }

    void stopWatch()
    {
        GUniqueOutPtr<GError> error;
        runJavaScriptAndWaitUntilFinished("navigator.geolocation.clearWatch(watchID)", &error.outPtr());
        g_assert_no_error(error.get());
        g_main_loop_run(m_mainLoop);
        m_watching = false;
    }

    WebKitGeolocationManager* m_manager;
    bool m_updating { false };
    bool m_watching { false };
    WebKitGeolocationPosition* m_checkPosition { nullptr };
    GUniquePtr<char> m_errorMessage;
};

static void testGeolocationManagerCurrentPosition(GeolocationTest* test, gconstpointer)
{
    test->showInWindow();
    test->loadHtml("<html><body></body></html>", "https://foo.com/bar");
    test->waitUntilLoadFinished();

    WebKitGeolocationPosition* position = webkit_geolocation_position_new(37.1760783, -3.59033, 17);
    g_assert_false(test->m_updating);
    auto result = test->checkCurrentPosition(position);
    g_assert_false(test->m_updating);
    g_assert_cmpfloat(result.latitude, ==, 37.1760783);
    g_assert_cmpfloat(result.longitude, ==, -3.59033);
    g_assert_cmpfloat(result.accuracy, ==, 17);
    g_assert_false(result.altitude);
    g_assert_false(result.altitudeAccuracy);
    g_assert_false(result.heading);
    g_assert_false(result.speed);
    g_assert_cmpfloat(result.timestamp, >, 0);
    g_assert_cmpfloat(result.timestamp / 1000., <, WallTime::now().secondsSinceEpoch().value());

    position = webkit_geolocation_position_new(27.986065, 86.922623, 0);
    auto timestamp = WallTime::now().secondsSinceEpoch();
    webkit_geolocation_position_set_timestamp(position, timestamp.value());
    webkit_geolocation_position_set_altitude(position, 8495);
    webkit_geolocation_position_set_altitude_accuracy(position, 0);
    webkit_geolocation_position_set_heading(position, 100.55);
    webkit_geolocation_position_set_speed(position, 0.05);
    g_assert_false(test->m_updating);
    result = test->checkCurrentPosition(position, true);
    g_assert_false(test->m_updating);
    g_assert_cmpfloat(result.latitude, ==, 27.986065);
    g_assert_cmpfloat(result.longitude, ==, 86.922623);
    g_assert_cmpfloat(result.accuracy, ==, 0);
    g_assert_true(result.altitude);
    g_assert_cmpfloat(result.altitude.value(), ==, 8495);
    g_assert_true(result.altitudeAccuracy);
    g_assert_cmpfloat(result.altitudeAccuracy.value(), ==, 0);
    g_assert_true(result.heading);
    g_assert_cmpfloat(result.heading.value(), ==, 100.55);
    g_assert_true(result.speed);
    g_assert_cmpfloat(result.speed.value(), ==, 0.05);
    g_assert_cmpfloat_with_epsilon(result.timestamp / 1000., timestamp.value(), 1);

    auto errorMessage = test->checkFailedToDeterminePosition("GPS is not active");
    g_assert_cmpstr(errorMessage.get(), ==, "GPS is not active");
}

static void testGeolocationManagerWatchPosition(GeolocationTest* test, gconstpointer)
{
    test->showInWindow();
    test->loadHtml("<html><body></body></html>", "https://foo.com/bar");
    test->waitUntilLoadFinished();

    test->startWatch();
    g_assert_true(test->m_updating);

    WebKitGeolocationPosition* position = webkit_geolocation_position_new(37.1760783, -3.59033, 17);
    webkit_geolocation_manager_update_position(test->m_manager, position);
    webkit_geolocation_position_free(position);
    auto result = test->lastPosition();
    g_assert_cmpfloat(result.latitude, ==, 37.1760783);
    g_assert_cmpfloat(result.longitude, ==, -3.59033);
    g_assert_cmpfloat(result.accuracy, ==, 17);

    position = webkit_geolocation_position_new(38.1770783, -3.60033, 17);
    webkit_geolocation_manager_update_position(test->m_manager, position);
    webkit_geolocation_position_free(position);
    result = test->lastPosition();
    g_assert_cmpfloat(result.latitude, ==, 38.1770783);
    g_assert_cmpfloat(result.longitude, ==, -3.60033);
    g_assert_cmpfloat(result.accuracy, ==, 17);

    test->stopWatch();
    g_assert_false(test->m_updating);
}

void beforeAll()
{
    GeolocationTest::add("WebKitGeolocationManager", "current-position", testGeolocationManagerCurrentPosition);
    GeolocationTest::add("WebKitGeolocationManager", "watch-position", testGeolocationManagerWatchPosition);
}

void afterAll()
{
}
