/*
 * Copyright (C) 2026 Igalia S.L.
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
#include <wtf/TimeZone.h>

#include "GLibMainLoopUtilities.h"
#include "Helpers/Test.h"
#include <gio/gio.h>

namespace TestWebKitAPI {

namespace {

// Returned variant carries a floating reference, intended to be sunk by
// `@a{sv}` in a g_variant_new() call.
GVariant* buildChangedDict(std::initializer_list<std::pair<const char*, const char*>> entries)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{sv}"));
    for (const auto& entry : entries)
        g_variant_builder_add(&builder, "{sv}", entry.first, g_variant_new_string(entry.second));
    return g_variant_builder_end(&builder);
}

// Returned variant carries a floating reference (see buildChangedDict).
GVariant* buildInvalidatedArray(std::initializer_list<const char*> names)
{
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
    for (const auto* name : names)
        g_variant_builder_add(&builder, "s", name);
    return g_variant_builder_end(&builder);
}

void emitPropertiesChanged(GDBusConnection* connection, GVariant* changed, GVariant* invalidated)
{
    g_dbus_connection_emit_signal(connection,
        nullptr,
        "/org/freedesktop/timedate1",
        "org.freedesktop.DBus.Properties",
        "PropertiesChanged",
        g_variant_new("(s@a{sv}@as)", "org.freedesktop.timedate1", changed, invalidated),
        nullptr);
}

// GBusNameAcquiredCallback: sets the bool pointed to by userData to true.
void nameAcquiredCallback(GDBusConnection*, const char*, gpointer userData)
{
    *static_cast<bool*>(userData) = true;
}

} // namespace

// listenForTimeZoneChangeNotifications() is std::call_once'd, so the whole
// fixture is bound to a single bus brought up in SetUpTestSuite and shared
// by every TEST_F below.
class WTF_TimeZoneGLib : public ::testing::Test {
public:
    static void SetUpTestSuite()
    {
        // Both the fixture and WTF_TimeZoneGLib_External below trip
        // listenForTimeZoneChangeNotifications()'s call_once. Letting both
        // run in one binary invocation would leak whichever bus loses the
        // race into the other path's tests and produce a 30s mystery hang.
        if (g_getenv("WEBKIT_TIMEDATE1_TEST_BUS")) {
            g_error("WEBKIT_TIMEDATE1_TEST_BUS is set, but the WTF_TimeZoneGLib fixture "
                "would steal listenForTimeZoneChangeNotifications()'s call_once from "
                "the external test. Re-run via Tools/Scripts/run-timezone-glib-e2e-test.");
        }

        s_bus = g_test_dbus_new(G_TEST_DBUS_NONE);
        g_test_dbus_up(s_bus);

        // The production proxy uses the SYSTEM bus; GTestDBus only writes
        // DBUS_SESSION_BUS_ADDRESS, so we point the system-bus var at the
        // same daemon to redirect g_bus_get(SYSTEM).
        const char* address = g_test_dbus_get_bus_address(s_bus);
        ASSERT_TRUE(address);
        g_setenv("DBUS_SYSTEM_BUS_ADDRESS", address, TRUE);

        GError* error = nullptr;
        s_serverConnection = g_dbus_connection_new_for_address_sync(address,
            static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
            nullptr, nullptr, &error);
        ASSERT_TRUE(s_serverConnection) << (error ? error->message : "no error");

        bool nameAcquired = false;
        s_ownerId = g_bus_own_name_on_connection(s_serverConnection,
            "org.freedesktop.timedate1",
            G_BUS_NAME_OWNER_FLAGS_NONE,
            nameAcquiredCallback,
            nullptr,
            &nameAcquired, nullptr);
        ASSERT_TRUE(runMainLoopUntil([&] {
            return nameAcquired;
        }));

        WTF::listenForTimeZoneChangeNotifications();

        // The subscription resolves the well-known name's unique owner via
        // an async GetNameOwner call. Signals emitted before that lands are
        // dropped at the filter stage, and there's no hook to observe the
        // resolution finishing, so probe-with-retry until a single Timezone
        // bump confirms the wiring is live. Each retry waits for its own
        // emit to land, so no stale signals leak into the per-test cases.
        uint64_t baseline = WTF::lastTimeZoneID();
        bool wired = false;
        for (unsigned attempt = 0; attempt < 50 && !wired; ++attempt) {
            emitPropertiesChanged(s_serverConnection,
                buildChangedDict({ { "Timezone", "Etc/UTC" } }),
                buildInvalidatedArray({ }));
            wired = runMainLoopUntil([&] {
                return WTF::lastTimeZoneID() > baseline;
            }, 1);
        }
        ASSERT_TRUE(wired) << "timedate1 subscription never observed our probe Timezone change";
    }

    static void TearDownTestSuite()
    {
        if (s_ownerId)
            g_bus_unown_name(s_ownerId);
        if (s_serverConnection) {
            g_dbus_connection_close_sync(s_serverConnection, nullptr, nullptr);
            g_object_unref(s_serverConnection);
        }
        if (s_bus) {
            g_test_dbus_down(s_bus);
            g_object_unref(s_bus);
        }
        s_ownerId = 0;
        s_serverConnection = nullptr;
        s_bus = nullptr;
    }

protected:
    static GTestDBus* s_bus;
    static GDBusConnection* s_serverConnection;
    static guint s_ownerId;
};

GTestDBus* WTF_TimeZoneGLib::s_bus = nullptr;
GDBusConnection* WTF_TimeZoneGLib::s_serverConnection = nullptr;
guint WTF_TimeZoneGLib::s_ownerId = 0;

TEST_F(WTF_TimeZoneGLib, FiresOnTimezonePropertyChange)
{
    uint64_t baseline = WTF::lastTimeZoneID();
    emitPropertiesChanged(s_serverConnection,
        buildChangedDict({ { "Timezone", "Europe/Madrid" } }),
        buildInvalidatedArray({ }));
    EXPECT_TRUE(runMainLoopUntil([&] {
        return WTF::lastTimeZoneID() > baseline;
    }));
}

// The freedesktop Properties spec lets services report a change via the
// invalidated-array (key only) instead of the changed-dict; missing that path
// would silently drop notifications from implementations that prefer it.
TEST_F(WTF_TimeZoneGLib, FiresOnInvalidatedTimezone)
{
    uint64_t baseline = WTF::lastTimeZoneID();
    emitPropertiesChanged(s_serverConnection,
        buildChangedDict({ }),
        buildInvalidatedArray({ "Timezone" }));
    EXPECT_TRUE(runMainLoopUntil([&] {
        return WTF::lastTimeZoneID() > baseline;
    }));
}

// timedate1 emits PropertiesChanged for NTP/LocalRTC too — those must NOT
// bump the time-zone ID, or every NTP toggle would force JSC's date cache
// to invalidate.
TEST_F(WTF_TimeZoneGLib, IgnoresUnrelatedProperties)
{
    uint64_t baseline = WTF::lastTimeZoneID();
    emitPropertiesChanged(s_serverConnection,
        buildChangedDict({ { "NTP", "true" }, { "LocalRTC", "false" } }),
        buildInvalidatedArray({ "CanNTP" }));
    EXPECT_FALSE(runMainLoopUntil([&] {
        return WTF::lastTimeZoneID() > baseline;
    }, 1));
}

// Driven by Tools/Scripts/run-timezone-glib-e2e-test, which owns timedate1
// from a separate process and emits PropertiesChanged across a real bus.
TEST(WTF_TimeZoneGLib_External, ReceivesExternalSignal)
{
    const char* externalBus = g_getenv("WEBKIT_TIMEDATE1_TEST_BUS");
    if (!externalBus) {
        // The WebKit gtest listener prints any non-Passed() result as **FAIL**,
        // so GTEST_SKIP would surface noise in default runs. Return cleanly
        // and rely on the driver to invoke us under the env var.
        SAFE_FPRINTF(stderr, "WTF_TimeZoneGLib_External: WEBKIT_TIMEDATE1_TEST_BUS unset; "
            "run via Tools/Scripts/run-timezone-glib-e2e-test to exercise.\n");
        return;
    }

    g_setenv("DBUS_SYSTEM_BUS_ADDRESS", externalBus, TRUE);
    WTF::listenForTimeZoneChangeNotifications();

    uint64_t baseline = WTF::lastTimeZoneID();
    EXPECT_TRUE(runMainLoopUntil([&] {
        return WTF::lastTimeZoneID() > baseline;
    }, 30));
}

} // namespace TestWebKitAPI
