/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, WA 02110-1301  USA
 */

#include "config.h"
#include "WebViewTest.h"

#include <wpe/GRefPtrWPE.h>
#include <wpe/wpe-platform.h>
#include <wtf/glib/GUniquePtr.h>

// WPEView::event-processed reports what the web content did with an input event
// it was given. A browser needs it to decide whether a shortcut the page did not
// take should be handled by the application instead, and whether a scroll or a
// tap the page ignored should be used for something else.
class EventProcessedTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(EventProcessedTest);

    ~EventProcessedTest()
    {
        if (m_signalID)
            g_signal_handler_disconnect(view(), m_signalID);
    }

    WPEView* view() const
    {
        auto* view = webkit_web_view_get_wpe_view(m_webView.get());
        g_assert_true(WPE_IS_VIEW(view));
        return view;
    }

    void loadContentsAndWait(const char* html, int width = 0, int height = 0)
    {
        if (!m_signalID)
            m_signalID = g_signal_connect(view(), "event-processed", G_CALLBACK(eventProcessedCallback), this);

        showInWindow(width, height);
        loadHtml(html, nullptr);
        waitUntilLoadFinished();
    }

    enum class Verdict { Pending, Handled, Declined };

    // Every key the web engine processes is reported, so this can wait for the
    // answer rather than guess at how long one might take to arrive.
    Verdict keyDownVerdict(unsigned keyVal, OptionSet<Modifiers> modifiers = OptionSet<Modifiers>())
    {
        m_verdict = Verdict::Pending;
        m_keyval = 0;
        keyStroke(keyVal, modifiers);

        unsigned triesCount = 100;
        while (m_verdict == Verdict::Pending && triesCount--)
            wait(0.05);

        return m_verdict;
    }

    unsigned reportedKeyval() const { return m_keyval; }

    Verdict touchDownVerdict(double x, double y)
    {
        m_verdict = Verdict::Pending;
        GRefPtr<WPEEvent> event = adoptGRef(wpe_event_touch_new(WPE_EVENT_TOUCH_DOWN, view(),
            WPE_INPUT_SOURCE_TOUCHSCREEN, 0, static_cast<WPEModifiers>(0), 0, x, y));
        wpe_view_event(view(), event.get());

        unsigned triesCount = 100;
        while (m_verdict == Verdict::Pending && triesCount--)
            wait(0.05);

        return m_verdict;
    }

private:
    static void eventProcessedCallback(WPEView*, WPEEvent* event, gboolean handled, EventProcessedTest* test)
    {
        if (wpe_event_get_event_type(event) == WPE_EVENT_TOUCH_DOWN) {
            test->m_verdict = handled ? Verdict::Handled : Verdict::Declined;
            return;
        }

        // The key up follows every key down, and only the key down decides
        // whether an accelerator should run.
        if (wpe_event_get_event_type(event) != WPE_EVENT_KEYBOARD_KEY_DOWN)
            return;

        // Reading the event here is what requires the UI process to keep it
        // alive: whoever delivered it dropped its reference long ago.
        test->m_keyval = wpe_event_keyboard_get_keyval(event);
        test->m_verdict = handled ? Verdict::Handled : Verdict::Declined;
    }

    unsigned long m_signalID { 0 };
    Verdict m_verdict { Verdict::Pending };
    unsigned m_keyval { 0 };
};

static const char* plainHTML = "<html><body>All work and no play.</body></html>";

static const char* indifferentListenerHTML =
    "<html><body>All work and no play."
    "<script>window.addEventListener('keydown', () => { window.seen = true; });</script>"
    "</body></html>";

static const char* preventDefaultHTML =
    "<html><body>All work and no play."
    "<script>window.addEventListener('keydown', e => {"
    "  if (e.ctrlKey && e.key === 'f') e.preventDefault();"
    "});</script></body></html>";

static void testKeyboardEventDeclinedByPage(EventProcessedTest* test, gconstpointer)
{
    test->loadContentsAndWait(plainHTML);

    g_assert_true(test->keyDownVerdict(WPE_KEY_f, { WebViewTest::Modifiers::Control }) == EventProcessedTest::Verdict::Declined);
    g_assert_cmpuint(test->reportedKeyval(), ==, WPE_KEY_f);
}

// A page that merely listens for a key has not claimed it. If this were
// reported as handled, no application could tell the two cases apart.
static void testKeyboardEventListenerAloneDoesNotClaimKey(EventProcessedTest* test, gconstpointer)
{
    test->loadContentsAndWait(indifferentListenerHTML);

    g_assert_true(test->keyDownVerdict(WPE_KEY_f, { WebViewTest::Modifiers::Control }) == EventProcessedTest::Verdict::Declined);
    test->assertJavaScriptBecomesTrue("window.seen === true");
}

static void testKeyboardEventHandledByPage(EventProcessedTest* test, gconstpointer)
{
    test->loadContentsAndWait(preventDefaultHTML);

    g_assert_true(test->keyDownVerdict(WPE_KEY_f, { WebViewTest::Modifiers::Control }) == EventProcessedTest::Verdict::Handled);
    g_assert_cmpuint(test->reportedKeyval(), ==, WPE_KEY_f);

    // The same page leaves every other key alone.
    g_assert_true(test->keyDownVerdict(WPE_KEY_g, { WebViewTest::Modifiers::Control }) == EventProcessedTest::Verdict::Declined);
    g_assert_cmpuint(test->reportedKeyval(), ==, WPE_KEY_g);
}

// Modifier presses are reported like any other key. Under the event
// reinjection this API replaces, these were the ones that looped forever.
static void testKeyboardEventModifierAlone(EventProcessedTest* test, gconstpointer)
{
    test->loadContentsAndWait(plainHTML);

    g_assert_true(test->keyDownVerdict(WPE_KEY_Control_L) == EventProcessedTest::Verdict::Declined);
    g_assert_cmpuint(test->reportedKeyval(), ==, WPE_KEY_Control_L);
}

// A key the engine acts on itself is handled, so it never reaches an
// application shortcut. Scrolling and editing commands both work this way.
static void testKeyboardEventHandledByEngine(EventProcessedTest* test, gconstpointer)
{
    test->loadContentsAndWait("<html><body style=\"height: 5000px\">All work and no play.</body></html>", 200, 200);

    g_assert_true(test->keyDownVerdict(WPE_KEY_Down) == EventProcessedTest::Verdict::Handled);
    test->assertJavaScriptBecomesTrue("window.scrollY > 0");

    g_assert_true(test->keyDownVerdict(WPE_KEY_a, { WebViewTest::Modifiers::Control }) == EventProcessedTest::Verdict::Handled);
}

// The same signal reports touch, which is how a platform learns that a tap
// went unused. This is the path WebDriver drives for pointerType "touch".
static void testTouchEventDeclinedByPage(EventProcessedTest* test, gconstpointer)
{
    test->loadContentsAndWait(plainHTML, 200, 200);

    g_assert_true(test->touchDownVerdict(20, 20) == EventProcessedTest::Verdict::Declined);
}

static void testTouchEventHandledByPage(EventProcessedTest* test, gconstpointer)
{
    test->loadContentsAndWait("<html><body>All work and no play."
        "<script>window.seen = false;"
        "window.addEventListener('touchstart', e => { window.seen = true; e.preventDefault(); }, { passive: false });"
        "</script></body></html>", 200, 200);

    // On the GLib ports touch events take the coordinated path, where the
    // scrolling thread answers from the scrolling tree's touch event regions.
    // Those only exist once a rendering update has pushed them there, so a
    // touch sent before that is answered declined without the page ever
    // seeing it.
    test->wait(1);

    auto verdict = test->touchDownVerdict(20, 20);
    test->assertJavaScriptBecomesTrue("window.seen === true");
    g_assert_true(verdict == EventProcessedTest::Verdict::Handled);
}

void beforeAll()
{
    EventProcessedTest::add("WebKitWebView", "keyboard-event-declined-by-page", testKeyboardEventDeclinedByPage);
    EventProcessedTest::add("WebKitWebView", "keyboard-event-listener-alone-does-not-claim-key", testKeyboardEventListenerAloneDoesNotClaimKey);
    EventProcessedTest::add("WebKitWebView", "keyboard-event-handled-by-page", testKeyboardEventHandledByPage);
    EventProcessedTest::add("WebKitWebView", "keyboard-event-modifier-alone", testKeyboardEventModifierAlone);
    EventProcessedTest::add("WebKitWebView", "keyboard-event-handled-by-engine", testKeyboardEventHandledByEngine);
    EventProcessedTest::add("WebKitWebView", "touch-event-declined-by-page", testTouchEventDeclinedByPage);
    EventProcessedTest::add("WebKitWebView", "touch-event-handled-by-page", testTouchEventHandledByPage);
}

void afterAll()
{
}
