/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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

#include "WebViewTest.h"
#include "WebXRTestHelpers.h"
#include <dlfcn.h>
#include <wtf/glib/GUniquePtr.h>

// This test exercises the UIProcess OpenXR instance lifecycle against an in-process mock
// OpenXR runtime (Tools/TestWebKitAPI/openxr-mock), selected via XR_RUNTIME_JSON so the
// production loader/dispatch path is unchanged.
struct MockEventState {
    int created { 0 };
    int destroyed { 0 };
    GMainLoop* loopToQuitOnDestroy { nullptr };
    bool timedOut { false };
};

static void mockEventCallback(const char* event, void* userData)
{
    auto* state = static_cast<MockEventState*>(userData);
    if (!g_strcmp0(event, "instance-created"))
        ++state->created;
    else if (!g_strcmp0(event, "instance-destroyed")) {
        ++state->destroyed;
        if (state->loopToQuitOnDestroy)
            g_main_loop_quit(state->loopToQuitOnDestroy);
    }
}

static gboolean destroyWaitTimeout(gpointer userData)
{
    auto* state = static_cast<MockEventState*>(userData);
    state->timedOut = true;
    if (state->loopToQuitOnDestroy)
        g_main_loop_quit(state->loopToQuitOnDestroy);
    return G_SOURCE_REMOVE;
}

using MockEventCallback = void (*)(const char* event, void* userData);

// Handle + the one entry point the test needs from the mock's exported control API.
struct MockOpenXRControl {
    void* handle { nullptr };
    void (*setEventCallback)(MockEventCallback, void*) { nullptr };
};

static MockOpenXRControl openMockOpenXRControl()
{
    // Load the OpenXR mock runtime before the OpenXR loader uses it for enumeration.
    GUniquePtr<char> manifestDir(g_path_get_dirname(MOCK_OPENXR_RUNTIME_JSON));
    GUniquePtr<char> modulePath(g_build_filename(manifestDir.get(), "libMockOpenXRRuntime.so", nullptr));
    void* handle = dlopen(modulePath.get(), RTLD_NOW);
    g_assert_nonnull(handle);

    void* setEventCallback = dlsym(handle, "mock_openxr_set_event_callback");
    g_assert_nonnull(setEventCallback);

    MockOpenXRControl control;
    control.handle = handle;
    control.setEventCallback = reinterpret_cast<void(*)(MockEventCallback, void*)>(setEventCallback);
    return control;
}

class WebXRInstanceLifecycleTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(WebXRInstanceLifecycleTest);

    WebXRInstanceLifecycleTest()
    {
        relaxDMABufRequirement(webkit_web_view_get_settings(m_webView.get()));
    }

    // Web process termination runs WebPageProxy::resetState, which calls PlatformXRSystem::invalidate(State).
    void terminateWebProcess()
    {
        m_expectedWebProcessCrash = true;
        webkit_web_view_terminate_web_process(m_webView.get());
    }

    bool waitForInstanceDestroyed(MockEventState& state)
    {
        if (state.destroyed)
            return true;
        state.loopToQuitOnDestroy = m_mainLoop;
        unsigned timeoutID = g_timeout_add_seconds(5, destroyWaitTimeout, &state);
        g_main_loop_run(m_mainLoop);
        if (!state.timedOut)
            g_source_remove(timeoutID);
        state.loopToQuitOnDestroy = nullptr;
        return state.destroyed;
    }
};

static void testOpenXRInstanceReleasedOnTeardown(WebXRInstanceLifecycleTest* test, gconstpointer)
{
    MockEventState events;
    auto control = openMockOpenXRControl();
    control.setEventCallback(mockEventCallback, &events);

    test->showInWindow();

    test->loadHtml("<html><body></body></html>", "https://foo.com/");
    test->waitUntilLoadFinished();

    // Device enumeration: creates an OpenXR instance in the UIProcess but starts no session, leaving PlatformXRSystem in the Idle state.
    test->runJavaScriptAndWaitUntilFinished(
        "navigator.xr.isSessionSupported('immersive-vr')"
        "  .then(s => { document.title = 'done:' + s; })"
        "  .catch(e => { document.title = 'error:' + e; });", nullptr);
    test->waitUntilTitleChanged();

    g_assert_cmpint(events.created, ==, 1);
    g_assert_cmpint(events.destroyed, ==, 0);

    // With no active session, invalidate(State) must release the instance.
    test->terminateWebProcess();

    g_assert_true(test->waitForInstanceDestroyed(events));
    g_assert_cmpint(events.destroyed, ==, 1);
    g_assert_cmpint(events.created, ==, 1);

    control.setEventCallback(nullptr, nullptr);
    dlclose(control.handle);
}

void beforeAll()
{
    // Select the in-process mock OpenXR runtime for this process before any OpenXR use.
    g_setenv("XR_RUNTIME_JSON", MOCK_OPENXR_RUNTIME_JSON, TRUE);

    WebXRInstanceLifecycleTest::add("OpenXR", "instance-released-on-teardown", testOpenXRInstanceReleasedOnTeardown);
}

void afterAll()
{
}
