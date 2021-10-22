/*
 * Copyright (C) 2012, 2019 Igalia S.L.
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

#include "WebViewTest.h"
#include <wtf/MonotonicTime.h>

// The libatspi headers don't use G_BEGIN_DECLS
extern "C" {
#include <atspi/atspi.h>
}

struct AtspiEventDeleter {
    void operator()(AtspiEvent* event) const
    {
        g_boxed_free(ATSPI_TYPE_EVENT, event);
    }
};

struct AtspiTextRangeDeleter {
    void operator()(AtspiTextRange* range) const
    {
        g_boxed_free(ATSPI_TYPE_TEXT_RANGE, range);
    }
};

using UniqueAtspiEvent = std::unique_ptr<AtspiEvent, AtspiEventDeleter>;
using UniqueAtspiTextRange = std::unique_ptr<AtspiTextRange, AtspiTextRangeDeleter>;

class AccessibilityTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(AccessibilityTest);

    GRefPtr<AtspiAccessible> findTestApplication()
    {
        // Only one desktop is supported by ATSPI at the moment.
        GRefPtr<AtspiAccessible> desktop = adoptGRef(atspi_get_desktop(0));

        // We can get warnings from atspi when trying to connect to applications.
        Test::removeLogFatalFlag(G_LOG_LEVEL_WARNING);
        int childCount = atspi_accessible_get_child_count(desktop.get(), nullptr);
        Test::addLogFatalFlag(G_LOG_LEVEL_WARNING);
        for (int i = 0; i < childCount; ++i) {
            GRefPtr<AtspiAccessible> current = adoptGRef(atspi_accessible_get_child_at_index(desktop.get(), i, nullptr));
            if (!g_strcmp0(atspi_accessible_get_name(current.get(), nullptr), "TestWebKitAccessibility"))
                return current;
        }

        return 0;
    }

    GRefPtr<AtspiAccessible> findDocumentWeb(AtspiAccessible* accessible)
    {
        int childCount = atspi_accessible_get_child_count(accessible, nullptr);
        for (int i = 0; i < childCount; ++i) {
            GRefPtr<AtspiAccessible> child = adoptGRef(atspi_accessible_get_child_at_index(accessible, i, nullptr));
            if (atspi_accessible_get_role(child.get(), nullptr) == ATSPI_ROLE_DOCUMENT_WEB)
                return child;

            if (auto documentWeb = findDocumentWeb(child.get()))
                return documentWeb;
        }
        return nullptr;
    }

    GRefPtr<AtspiAccessible> findRootObject(AtspiAccessible* application)
    {
        // Find the document web, its parent is the scroll view (WebCore root object) and its parent is
        // the GtkPlug (WebProcess root element).
        auto documentWeb = findDocumentWeb(application);
        if (!documentWeb)
            return nullptr;

        auto parent = adoptGRef(atspi_accessible_get_parent(documentWeb.get(), nullptr));
        return parent ? adoptGRef(atspi_accessible_get_parent(parent.get(), nullptr)) : nullptr;
    }

    void waitUntilChildrenRemoved(AtspiAccessible* accessible)
    {
        m_eventSource = accessible;
        GRefPtr<AtspiEventListener> listener = adoptGRef(atspi_event_listener_new(
            [](AtspiEvent* event, gpointer userData) {
                auto* test = static_cast<AccessibilityTest*>(userData);
                if (event->source == test->m_eventSource)
                    g_main_loop_quit(test->m_mainLoop);
        }, this, nullptr));
        atspi_event_listener_register(listener.get(), "object:children-changed:remove", nullptr);
        g_main_loop_run(m_mainLoop);
        m_eventSource = nullptr;
    }

    void startEventMonitor(AtspiAccessible* source, Vector<CString>&& events)
    {
        m_eventMonitor.source = source;
        m_eventMonitor.eventTypes = WTFMove(events);
        m_eventMonitor.listener = adoptGRef(atspi_event_listener_new([](AtspiEvent* event, gpointer userData) {
            auto* test = static_cast<AccessibilityTest*>(userData);
            if (event->source == test->m_eventMonitor.source)
                test->m_eventMonitor.events.append(static_cast<AtspiEvent*>(g_boxed_copy(ATSPI_TYPE_EVENT, event)));
        }, this, nullptr));

        for (const auto& event : m_eventMonitor.eventTypes)
            atspi_event_listener_register(m_eventMonitor.listener.get(), event.data(), nullptr);
    }

    Vector<UniqueAtspiEvent> stopEventMonitor(unsigned expectedEvents, std::optional<Seconds> timeout = std::nullopt)
    {
        // If events is empty wait for the events.
        auto startTime = MonotonicTime::now();
        while (m_eventMonitor.events.size() < expectedEvents && MonotonicTime::now() - startTime < timeout.value_or(Seconds::infinity()))
            g_main_context_iteration(nullptr, timeout ? FALSE : TRUE);

        auto events = WTFMove(m_eventMonitor.events);
        for (const auto& event : m_eventMonitor.eventTypes)
            atspi_event_listener_deregister(m_eventMonitor.listener.get(), event.data(), nullptr);
        m_eventMonitor = { nullptr, { }, { }, nullptr };
        return events;
    }

    static AtspiEvent* findEvent(const Vector<UniqueAtspiEvent>& events, const char* eventType)
    {
        for (const auto& event : events) {
            if (!g_strcmp0(event->type, eventType))
                return event.get();
        }

        return nullptr;
    }

    static unsigned stateSetSize(AtspiStateSet* stateSet)
    {
        GArray* states = atspi_state_set_get_states(stateSet);
        unsigned length = states->len;
        g_array_free(states, TRUE);
        return length;
    }

private:
    AtspiAccessible* m_eventSource { nullptr };

    struct {
        GRefPtr<AtspiEventListener> listener;
        Vector<CString> eventTypes;
        Vector<UniqueAtspiEvent> events;
        AtspiAccessible* source { nullptr };
    } m_eventMonitor;
};

static void testAccessibleBasicHierarchy(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow();
    test->loadHtml(
        "<html>"
        "  <body>"
        "   <h1>This is a test</h1>"
        "   <p>This is a paragraph with some plain text.</p>"
        "   <p>This paragraph contains <a href=\"http://www.webkitgtk.org\">a link</a> in the middle.</p>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));
    GUniquePtr<char> name(atspi_accessible_get_name(testApp.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "TestWebKitAccessibility");
    g_assert_cmpint(atspi_accessible_get_role(testApp.get(), nullptr), ==, ATSPI_ROLE_APPLICATION);

    auto rootObject = test->findRootObject(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(rootObject.get()));
    g_assert_cmpint(atspi_accessible_get_role(rootObject.get(), nullptr), ==, ATSPI_ROLE_FILLER);

    auto scrollView = adoptGRef(atspi_accessible_get_child_at_index(rootObject.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(scrollView.get()));
    g_assert_cmpint(atspi_accessible_get_role(scrollView.get(), nullptr), ==, ATSPI_ROLE_SCROLL_PANE);

    auto documentWeb = adoptGRef(atspi_accessible_get_child_at_index(scrollView.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_role(documentWeb.get(), nullptr), ==, ATSPI_ROLE_DOCUMENT_WEB);

    auto h1 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(h1.get()));
    name.reset(atspi_accessible_get_name(h1.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "This is a test");
    g_assert_cmpint(atspi_accessible_get_role(h1.get(), nullptr), ==, ATSPI_ROLE_HEADING);
    name.reset(atspi_accessible_get_localized_role_name(h1.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "heading");

    auto p1 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(p1.get()));
    g_assert_cmpint(atspi_accessible_get_role(p1.get(), nullptr), ==, ATSPI_ROLE_PARAGRAPH);
    name.reset(atspi_accessible_get_localized_role_name(p1.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "paragraph");

    auto p2 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 2, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(p2.get()));
    g_assert_cmpint(atspi_accessible_get_role(p2.get(), nullptr), ==, ATSPI_ROLE_PARAGRAPH);
    name.reset(atspi_accessible_get_localized_role_name(p2.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "paragraph");

    auto link = adoptGRef(atspi_accessible_get_child_at_index(p2.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(link.get()));
    name.reset(atspi_accessible_get_name(link.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "a link");
    g_assert_cmpint(atspi_accessible_get_role(link.get(), nullptr), ==, ATSPI_ROLE_LINK);
    name.reset(atspi_accessible_get_localized_role_name(link.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "link");

    test->loadHtml(
        "<html>"
        "  <body>"
        "   <h1>This is another test</h1>"
        "   <img src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAACXBIWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3AYWDTMVwnSZnwAAAB1pVFh0Q29tbWVudAAAAAAAQ3JlYXRlZCB3aXRoIEdJTVBkLmUHAAAAFklEQVQI12P8z8DAwMDAxMDAwMDAAAANHQEDK+mmyAAAAABJRU5ErkJggg=='/>"
        "  </body>"
        "</html>",
        nullptr);
#if USE(ATSPI)
    // In atspi implementation the root object doesn't emit children-changed because it
    // always has ManagesDescendants in state.
    test->waitUntilLoadFinished();
#else
    // Check that children-changed::remove is emitted on the root object on navigation,
    // and the a11y hierarchy is updated.
    test->waitUntilChildrenRemoved(rootObject.get());
#endif

    documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_role(documentWeb.get(), nullptr), ==, ATSPI_ROLE_DOCUMENT_WEB);

    h1 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(h1.get()));
    name.reset(atspi_accessible_get_name(h1.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "This is another test");
    g_assert_cmpint(atspi_accessible_get_role(h1.get(), nullptr), ==, ATSPI_ROLE_HEADING);

    auto section = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(section.get()));
    g_assert_cmpint(atspi_accessible_get_role(section.get(), nullptr), ==, ATSPI_ROLE_SECTION);

    auto img = adoptGRef(atspi_accessible_get_child_at_index(section.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(img.get()));
    g_assert_cmpint(atspi_accessible_get_role(img.get(), nullptr), ==, ATSPI_ROLE_IMAGE);
}

static void testAccessibleIgnoredObjects(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow();
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <div>"
        "      <p>Static text nodes are ignored, so this paragraph shouldn't have a child</p>"
        "    </div>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    auto p = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(p.get()));
    g_assert_cmpint(atspi_accessible_get_role(p.get(), nullptr), ==, ATSPI_ROLE_PARAGRAPH);
    g_assert_cmpint(atspi_accessible_get_child_count(p.get(), nullptr), ==, 0);
}

static void testAccessibleChildrenChanged(AccessibilityTest* test, gconstpointer)
{
#if !USE(ATSPI)
    g_test_skip("This test doesn't work with ATK");
#else
    test->showInWindow();
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <div id='grandparent'>"
        "      <div id='parent'>"
        "        <p>Foo</p><p>Bar</p>"
        "      </div>"
        "    </div>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    // The divs are not exposed to ATs.
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 2);

    auto foo = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(foo.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(foo.get(), nullptr), ==, 0);

    auto bar = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(bar.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(bar.get(), nullptr), ==, 0);

    // Add a new paragraph.
    test->startEventMonitor(documentWeb.get(), { "object:children-changed:add", "object:children-changed:remove" });
    test->runJavaScriptAndWaitUntilFinished("let p = document.createElement('p'); p.innerText = 'Baz'; document.getElementById('parent').appendChild(p);", nullptr);
    auto events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:children-changed:add");
    g_assert_cmpuint(events[0]->detail1, ==, 2); // New node added to position 2.
    g_assert_true(G_VALUE_HOLDS_OBJECT(&events[0]->any_data));
    auto baz = adoptGRef(static_cast<AtspiAccessible*>(g_value_dup_object(&events[0]->any_data)));
    g_assert_true(ATSPI_IS_ACCESSIBLE(baz.get()));
    g_assert_cmpint(atspi_accessible_get_role(baz.get(), nullptr), ==, ATSPI_ROLE_PARAGRAPH);
    g_assert_true(atspi_accessible_get_parent(baz.get(), nullptr) == documentWeb.get());
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 3);
    GRefPtr<AtspiAccessible> child = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(foo.get() == child.get());
    child = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(bar.get() == child.get());
    child = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 2, nullptr));
    g_assert_true(baz.get() == child.get());
    child = nullptr;
    events = { };

    // Remove one of the paragraphs.
    test->startEventMonitor(documentWeb.get(), { "object:children-changed:add", "object:children-changed:remove" });
    test->runJavaScriptAndWaitUntilFinished("let div = document.getElementById('parent'); div.removeChild(div.children[0]);", nullptr);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:children-changed:remove");
    g_assert_cmpuint(events[0]->detail1, ==, 0); // Remove node was at position 0.
    g_assert_true(G_VALUE_HOLDS_OBJECT(&events[0]->any_data));
    g_assert_true(g_value_get_object(&events[0]->any_data) == foo.get());
    GRefPtr<AtspiStateSet> set = adoptGRef(atspi_accessible_get_state_set(foo.get()));
    g_assert_true(atspi_state_set_contains(set.get(), ATSPI_STATE_DEFUNCT));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 2);
    child = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(bar.get() == child.get());
    child = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(baz.get() == child.get());
    child = nullptr;
    events = { };

    // Changing role causes an internal detach + attach that shouldn't be exposed to ATs.
    test->startEventMonitor(documentWeb.get(), { "object:children-changed:add", "object:children-changed:remove" });
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('parent').children[0].role='button'", nullptr);
    events = test->stopEventMonitor(0, 0.5_s);
    g_assert_cmpuint(events.size(), ==, 0);
    g_assert_cmpint(atspi_accessible_get_role(bar.get(), nullptr), ==, ATSPI_ROLE_PUSH_BUTTON);
    events = { };

    // Remove the container.
    test->startEventMonitor(documentWeb.get(), { "object:children-changed:add", "object:children-changed:remove" });
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('grandparent').removeChild(document.getElementById('parent'));", nullptr);
    events = test->stopEventMonitor(2);
    g_assert_cmpuint(events.size(), ==, 2);
    g_assert_cmpstr(events[0]->type, ==, "object:children-changed:remove");
    g_assert_cmpstr(events[1]->type, ==, "object:children-changed:remove");
    g_assert_true(G_VALUE_HOLDS_OBJECT(&events[0]->any_data));
    g_assert_true(G_VALUE_HOLDS_OBJECT(&events[1]->any_data));
    g_assert_true(g_value_get_object(&events[0]->any_data) == bar.get() || g_value_get_object(&events[0]->any_data) == baz.get());
    g_assert_true(g_value_get_object(&events[1]->any_data) == bar.get() || g_value_get_object(&events[1]->any_data) == baz.get());
    g_assert_true(g_value_get_object(&events[0]->any_data) != g_value_get_object(&events[1]->any_data));
    set = adoptGRef(atspi_accessible_get_state_set(bar.get()));
    g_assert_true(atspi_state_set_contains(set.get(), ATSPI_STATE_DEFUNCT));
    set = adoptGRef(atspi_accessible_get_state_set(baz.get()));
    g_assert_true(atspi_state_set_contains(set.get(), ATSPI_STATE_DEFUNCT));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 0);
#endif
}

static void testAccessibleAttributes(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow();
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <h2>Heading level 2</h2>"
        "    <a id=\"webkitgtk\" href=\"http://www.webkitgtk.org\">A link</a>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

#if USE(ATSPI)
    static const char* toolkitName = "WebKitGTK";
#else
    static const char* toolkitName = "WebKitGtk";
#endif

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 2);
    auto attributes = adoptGRef(atspi_accessible_get_attributes(documentWeb.get(), nullptr));
#if USE(ATSPI)
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 1);
#else
    // FIXME: ATK includes dragged state, but dragging is not supported.
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 2);
#endif
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "toolkit")), ==, toolkitName);

    auto h2 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(h2.get()));
    attributes = adoptGRef(atspi_accessible_get_attributes(h2.get(), nullptr));
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 4);
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "toolkit")), ==, toolkitName);
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "computed-role")), ==, "heading");
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "level")), ==, "2");
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "tag")), ==, "h2");

    auto section = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(section.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(section.get(), nullptr), ==, 1);

    auto a = adoptGRef(atspi_accessible_get_child_at_index(section.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(a.get()));
    attributes = adoptGRef(atspi_accessible_get_attributes(a.get(), nullptr));
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 4);
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "toolkit")), ==, toolkitName);
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "computed-role")), ==, "link");
#if USE(ATSPI)
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "id")), ==, "webkitgtk");
#else
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "html-id")), ==, "webkitgtk");
#endif
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "tag")), ==, "a");
}

static void testAccessibleState(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <h1>State</h1>"
        "    <button>Button</button>"
        "    <button disabled>Disabled</button>"
        "    <button aria-pressed='true'>Toggle button</button>"
        "    <input type='radio' name='radio'/>Radio"
        "    <input type='radio' name='radio' checked/>Radio Checked"
        "    <input type='radio' name='radio' disabled/>Radio disabled"
        "    <input type='checkbox' name='check-disabled' checked disabled/>Checked Disabled"
        "    <input value='Entry' aria-required='true'>"
        "    <input value='Disabled' disabled>"
        "    <input value='Read only' readonly>"
        "    <textarea rows=5 autofocus></textarea>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 2);

    auto h1 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(h1.get()));
    GRefPtr<AtspiStateSet> stateSet = adoptGRef(atspi_accessible_get_state_set(h1.get()));
#if USE(ATSPI)
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 4);
#else
    // FIXME: ATK includes orientation state in every element.
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 5);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_HORIZONTAL));
#endif
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_ENABLED));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SENSITIVE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));

    auto section = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(section.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(section.get(), nullptr), ==, 11);

    unsigned nextChild = 0;
    auto button = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(button.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(button.get()));
#if USE(ATSPI)
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 5);
#else
    // FIXME: ATK includes orientation state in every element.
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 6);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_HORIZONTAL));
#endif
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_ENABLED));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SENSITIVE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_FOCUSABLE));

    auto buttonDisabled = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(buttonDisabled.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(buttonDisabled.get()));
#if USE(ATSPI)
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 2);
#else
    // FIXME: ATK includes orientation state in every element.
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 3);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_HORIZONTAL));
#endif
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));

    auto toggleButton = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(toggleButton.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(toggleButton.get()));
#if USE(ATSPI)
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 6);
#else
    // FIXME: ATK includes orientation state in every element.
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 7);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_HORIZONTAL));
#endif
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_ENABLED));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SENSITIVE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_FOCUSABLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_PRESSED));

    auto radio = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(radio.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(radio.get()));
#if USE(ATSPI)
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 6);
#else
    // FIXME: ATK includes orientation state in every element.
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 7);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VERTICAL));
#endif
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_ENABLED));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SENSITIVE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_FOCUSABLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_CHECKABLE));

    auto radioChecked = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(radioChecked.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(radioChecked.get()));
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 7);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_ENABLED));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SENSITIVE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_FOCUSABLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_CHECKABLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_CHECKED));

    auto radioDisabled = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(radioDisabled.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(radioDisabled.get()));
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 3);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_CHECKABLE));

    auto checkbox = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(checkbox.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(checkbox.get()));
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 4);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_CHECKABLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_CHECKED));

    auto entry = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(entry.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(entry.get()));
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 9);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_ENABLED));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SENSITIVE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_FOCUSABLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_EDITABLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SINGLE_LINE));
#if USE(ATSPI)
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SELECTABLE_TEXT));
#else
    // FIXME: ATK doesn't implement selectable text, but includes oprientation.
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_HORIZONTAL));
#endif
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_REQUIRED));

    auto entryDisabled = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(entryDisabled.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(entryDisabled.get()));
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 5);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_EDITABLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SINGLE_LINE));
#if USE(ATSPI)
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SELECTABLE_TEXT));
#else
    // FIXME: ATK doesn't implement selectable text, but includes oprientation.
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_HORIZONTAL));
#endif

    auto entryReadOnly = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(entryReadOnly.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(entryReadOnly.get()));
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 8);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_ENABLED));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SENSITIVE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_FOCUSABLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SINGLE_LINE));
#if USE(ATSPI)
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SELECTABLE_TEXT));
#else
    // FIXME: ATK doesn't implement selectable text, but includes oprientation.
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_HORIZONTAL));
#endif
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_READ_ONLY));

    auto textArea = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(textArea.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(textArea.get()));
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 9);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_ENABLED));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SENSITIVE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_FOCUSABLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_FOCUSED));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_EDITABLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_MULTI_LINE));
#if USE(ATSPI)
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SELECTABLE_TEXT));
#else
    // FIXME: ATK doesn't implement selectable text, but includes oprientation.
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_HORIZONTAL));
#endif
}

static void testAccessibleStateChanged(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow();
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <input id='check' type='checkbox'/>Checkbox"
        "    <button id='toggle' aria-pressed='true'>Toggle button</button>"
        "    <input id='entry' value='Entry'>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    auto section = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(section.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(section.get(), nullptr), ==, 3);

    unsigned nextChild = 0;
    auto checkbox = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(checkbox.get()));
    test->startEventMonitor(checkbox.get(), { "object:state-changed" });
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('check').checked = true;", nullptr);
    auto events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:state-changed:checked");
    g_assert_cmpuint(events[0]->detail1, ==, 1);
    events = { };

    auto toggleButton = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(toggleButton.get()));
    test->startEventMonitor(toggleButton.get(), { "object:state-changed" });
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('toggle').ariaPressed = false;", nullptr);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:state-changed:pressed");
    g_assert_cmpuint(events[0]->detail1, ==, 0);
    events = { };

    auto entry = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(entry.get()));
    test->startEventMonitor(entry.get(), { "object:state-changed" });
    test->runJavaScriptAndWaitUntilFinished("let e = document.getElementById('entry'); e.ariaRequired = true; e.focus();", nullptr);
    events = test->stopEventMonitor(2);
#if !USE(ATSPI)
    // FIXME: With ATK focused event is sometimes emitted twice for some reason.
    if (events.size() == 3) {
        events.removeFirstMatching([](auto& item) {
            if (!strcmp(item->type, "object:state-changed:focused"))
                return true;
            return false;
        });
    }
#endif
    g_assert_cmpuint(events.size(), ==, 2);
    if (!g_strcmp0(events[0]->type, "object:state-changed:focused")) {
        g_assert_cmpuint(events[0]->detail1, ==, 1);

        g_assert_cmpstr(events[1]->type, ==, "object:state-changed:required");
        g_assert_cmpuint(events[1]->detail1, ==, 1);
    } else if (!g_strcmp0(events[0]->type, "object:state-changed:required")) {
        g_assert_cmpuint(events[0]->detail1, ==, 1);

        g_assert_cmpstr(events[1]->type, ==, "object:state-changed:focused");
        g_assert_cmpuint(events[1]->detail1, ==, 1);
    } else
        g_assert_not_reached();
    events = { };
}

static void testComponentHitTest(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow();
    GUniquePtr<char> baseDir(g_strdup_printf("file://%s/", Test::getResourcesDir().data()));
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <img style='position:absolute; left:1; top:1' src='blank.ico' width=5 height=5></img>"
        "  </body>"
        "</html>",
        baseDir.get());
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    auto img = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_COMPONENT(img.get()));
    GUniquePtr<AtspiRect> rect(atspi_component_get_extents(ATSPI_COMPONENT(img.get()), ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_nonnull(rect.get());
    g_assert_cmpuint(rect->x, ==, 1);
    g_assert_cmpuint(rect->y, ==, 1);
    g_assert_cmpuint(rect->width, ==, 5);
    g_assert_cmpuint(rect->height, ==, 5);
    GUniquePtr<AtspiPoint> point(atspi_component_get_position(ATSPI_COMPONENT(img.get()), ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_nonnull(point.get());
    g_assert_cmpuint(rect->x, ==, point->x);
    g_assert_cmpuint(rect->y, ==, point->y);
    GUniquePtr<AtspiPoint> size(atspi_component_get_size(ATSPI_COMPONENT(img.get()), nullptr));
    g_assert_nonnull(size.get());
    g_assert_cmpuint(size->x, ==, rect->width);
    g_assert_cmpuint(size->y, ==, rect->height);
    g_assert_true(atspi_component_contains(ATSPI_COMPONENT(img.get()), rect->x, rect->y, ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_false(atspi_component_contains(ATSPI_COMPONENT(img.get()), rect->x + rect->width, rect->y + rect->height, ATSPI_COORD_TYPE_WINDOW, nullptr));
    auto accessible = adoptGRef(atspi_component_get_accessible_at_point(ATSPI_COMPONENT(documentWeb.get()), rect->x, rect->y, ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_true(accessible.get() == img.get());
    accessible = adoptGRef(atspi_component_get_accessible_at_point(ATSPI_COMPONENT(documentWeb.get()), rect->x + rect->width, rect->y + rect->height, ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_true(accessible.get() == documentWeb.get());
}

#ifdef ATSPI_SCROLLTYPE_COUNT
static void testComponentScrollTo(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(640, 480);
    GUniquePtr<char> baseDir(g_strdup_printf("file://%s/", Test::getResourcesDir().data()));
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>Top</p>"
        "    <img src='blank.ico' width=200 height=600></img>"
        "    <p>Bottom</p>"
        "  </body>"
        "</html>",
        baseDir.get());
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 3);

    auto top = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_COMPONENT(top.get()));
    GUniquePtr<AtspiPoint> topPositionBeforeScrolling(atspi_component_get_position(ATSPI_COMPONENT(top.get()), ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_cmpint(topPositionBeforeScrolling->y, >, 0);
    g_assert_cmpint(topPositionBeforeScrolling->y, <, 480);

    auto bottom = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 2, nullptr));
    g_assert_true(ATSPI_IS_COMPONENT(bottom.get()));
    GUniquePtr<AtspiPoint> bottomPositionBeforeScrolling(atspi_component_get_position(ATSPI_COMPONENT(bottom.get()), ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_cmpint(bottomPositionBeforeScrolling->y, >, 480);

    atspi_component_scroll_to(ATSPI_COMPONENT(bottom.get()), ATSPI_SCROLL_ANYWHERE, nullptr);

    GUniquePtr<AtspiPoint> topPositionAfterScrolling(atspi_component_get_position(ATSPI_COMPONENT(top.get()), ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_cmpint(topPositionAfterScrolling->y, <, 0);
    GUniquePtr<AtspiPoint> bottomPositionAfterScrolling(atspi_component_get_position(ATSPI_COMPONENT(bottom.get()), ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_cmpint(bottomPositionAfterScrolling->y, <, 480);

    atspi_component_scroll_to_point(ATSPI_COMPONENT(top.get()), ATSPI_COORD_TYPE_WINDOW, topPositionBeforeScrolling->x, topPositionBeforeScrolling->y, nullptr);
    topPositionAfterScrolling.reset(atspi_component_get_position(ATSPI_COMPONENT(top.get()), ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_cmpint(topPositionBeforeScrolling->x, ==, topPositionAfterScrolling->x);
    g_assert_cmpint(topPositionBeforeScrolling->y, ==, topPositionAfterScrolling->y);
}
#endif

static void testTextBasic(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow();
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>This is a line of text</p>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    auto p = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(p.get()));
    auto length = atspi_text_get_character_count(ATSPI_TEXT(p.get()), nullptr);
    g_assert_cmpint(length, ==, 22);
    GUniquePtr<char> text(atspi_text_get_text(ATSPI_TEXT(p.get()), 0, length, nullptr));
    g_assert_cmpstr(text.get(), ==, "This is a line of text");
    text.reset(atspi_text_get_text(ATSPI_TEXT(p.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "This is a line of text");
    text.reset(atspi_text_get_text(ATSPI_TEXT(p.get()), 5, 7, nullptr));
    g_assert_cmpstr(text.get(), ==, "is");
    text.reset(atspi_text_get_text(ATSPI_TEXT(p.get()), 0, -2, nullptr));
    g_assert_cmpstr(text.get(), ==, "");
    text.reset(atspi_text_get_text(ATSPI_TEXT(p.get()), -5, 23, nullptr));
    g_assert_cmpstr(text.get(), ==, "");
}

static void testTextSurrogatePair(AccessibilityTest* test, gconstpointer)
{
#if USE(ATK)
    g_test_skip("Surrogate pairs are not correctly handled by WebKit when using ATK");
    return;
#endif

    test->showInWindow();
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>This contains a &#x1D306; symbol</p>"
        "    <input value='This contains a &#x1D306; symbol'/>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 2);

    auto p = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(p.get()));
    auto length = atspi_text_get_character_count(ATSPI_TEXT(p.get()), nullptr);
    g_assert_cmpint(length, ==, 24);
    GUniquePtr<char> text(atspi_text_get_text(ATSPI_TEXT(p.get()), 0, length, nullptr));
    g_assert_cmpstr(text.get(), ==, "This contains a 𝌆 symbol");
    text.reset(atspi_text_get_text(ATSPI_TEXT(p.get()), 16, 17, nullptr));
    g_assert_cmpstr(text.get(), ==, "𝌆");

    auto section = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(section.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(section.get(), nullptr), ==, 1);
    auto input = adoptGRef(atspi_accessible_get_child_at_index(section.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(input.get()));
    length = atspi_text_get_character_count(ATSPI_TEXT(input.get()), nullptr);
    g_assert_cmpint(length, ==, 24);
    text.reset(atspi_text_get_text(ATSPI_TEXT(input.get()), 0, length, nullptr));
    g_assert_cmpstr(text.get(), ==, "This contains a 𝌆 symbol");
    text.reset(atspi_text_get_text(ATSPI_TEXT(input.get()), 16, 17, nullptr));
    g_assert_cmpstr(text.get(), ==, "𝌆");
}

static void testTextIterator(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>Text of first sentence.   This is the second sentence.<br>And this is the next paragraph.</p>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    auto p = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(p.get()));
    auto length = atspi_text_get_character_count(ATSPI_TEXT(p.get()), nullptr);
    g_assert_cmpint(length, ==, 84);
    GUniquePtr<char> text(atspi_text_get_text(ATSPI_TEXT(p.get()), 0, length, nullptr));
    g_assert_cmpstr(text.get(), ==, "Text of first sentence. This is the second sentence.\nAnd this is the next paragraph.");

    // Character granularity.
    UniqueAtspiTextRange range(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 0, ATSPI_TEXT_GRANULARITY_CHAR, nullptr));
    g_assert_cmpstr(range->content, ==, "T");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 1);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 83, ATSPI_TEXT_GRANULARITY_CHAR, nullptr));
    g_assert_cmpstr(range->content, ==, ".");
    g_assert_cmpint(range->start_offset, ==, 83);
    g_assert_cmpint(range->end_offset, ==, 84);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 84, ATSPI_TEXT_GRANULARITY_CHAR, nullptr));
    g_assert_cmpstr(range->content, ==, "");
    g_assert_cmpint(range->start_offset, ==, 84);
    g_assert_cmpint(range->end_offset, ==, 84);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 85, ATSPI_TEXT_GRANULARITY_CHAR, nullptr));
    g_assert_cmpstr(range->content, ==, "");
#if USE(ATSPI)
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 0);
#else
    // FIXME: ATK returns wrong offsets in this case.
    g_assert_cmpint(range->start_offset, ==, 84);
    g_assert_cmpint(range->end_offset, ==, 84);
#endif
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), -1, ATSPI_TEXT_GRANULARITY_CHAR, nullptr));
    g_assert_cmpstr(range->content, ==, "");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 0);

    // Word granularity.
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 0, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "Text ");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 5);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 4, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "Text ");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 5);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 5, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "of ");
    g_assert_cmpint(range->start_offset, ==, 5);
    g_assert_cmpint(range->end_offset, ==, 8);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 40, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "second ");
    g_assert_cmpint(range->start_offset, ==, 36);
    g_assert_cmpint(range->end_offset, ==, 43);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 16, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "sentence. ");
    g_assert_cmpint(range->start_offset, ==, 14);
    g_assert_cmpint(range->end_offset, ==, 24);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 74, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "paragraph.");
    g_assert_cmpint(range->start_offset, ==, 74);
    g_assert_cmpint(range->end_offset, ==, 84);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 83, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "paragraph.");
    g_assert_cmpint(range->start_offset, ==, 74);
    g_assert_cmpint(range->end_offset, ==, 84);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 80, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "paragraph.");
    g_assert_cmpint(range->start_offset, ==, 74);
    g_assert_cmpint(range->end_offset, ==, 84);

    // Sentence granularity.
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 0, ATSPI_TEXT_GRANULARITY_SENTENCE, nullptr));
    g_assert_cmpstr(range->content, ==, "Text of first sentence. ");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 24);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 23, ATSPI_TEXT_GRANULARITY_SENTENCE, nullptr));
    g_assert_cmpstr(range->content, ==, "Text of first sentence. ");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 24);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 24, ATSPI_TEXT_GRANULARITY_SENTENCE, nullptr));
    g_assert_cmpstr(range->content, ==, "This is the second sentence.\n");
    g_assert_cmpint(range->start_offset, ==, 24);
    g_assert_cmpint(range->end_offset, ==, 53);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 40, ATSPI_TEXT_GRANULARITY_SENTENCE, nullptr));
    g_assert_cmpstr(range->content, ==, "This is the second sentence.\n");
    g_assert_cmpint(range->start_offset, ==, 24);
    g_assert_cmpint(range->end_offset, ==, 53);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 53, ATSPI_TEXT_GRANULARITY_SENTENCE, nullptr));
    g_assert_cmpstr(range->content, ==, "And this is the next paragraph.");
    g_assert_cmpint(range->start_offset, ==, 53);
    g_assert_cmpint(range->end_offset, ==, 84);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 83, ATSPI_TEXT_GRANULARITY_SENTENCE, nullptr));
    g_assert_cmpstr(range->content, ==, "And this is the next paragraph.");
    g_assert_cmpint(range->start_offset, ==, 53);
    g_assert_cmpint(range->end_offset, ==, 84);

    // Line granularity.
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 0, ATSPI_TEXT_GRANULARITY_LINE, nullptr));
    g_assert_cmpstr(range->content, ==, "Text of first sentence. This is the second sentence.\n");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 53);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 83, ATSPI_TEXT_GRANULARITY_LINE, nullptr));
    g_assert_cmpstr(range->content, ==, "And this is the next paragraph.");
    g_assert_cmpint(range->start_offset, ==, 53);
    g_assert_cmpint(range->end_offset, ==, 84);

#if USE(ATSPI) // ATK doesn't implement paragraph granularity.
    // Paragraph granularity.
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 0, ATSPI_TEXT_GRANULARITY_PARAGRAPH, nullptr));
    g_assert_cmpstr(range->content, ==, "Text of first sentence. This is the second sentence.");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 52);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 83, ATSPI_TEXT_GRANULARITY_PARAGRAPH, nullptr));
    g_assert_cmpstr(range->content, ==, "And this is the next paragraph.");
    g_assert_cmpint(range->start_offset, ==, 53);
    g_assert_cmpint(range->end_offset, ==, 84);
#endif

    // Using a text control now.
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <input value='Text of input field'/>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    auto section = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(section.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(section.get(), nullptr), ==, 1);
    auto input = adoptGRef(atspi_accessible_get_child_at_index(section.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(input.get()));
    length = atspi_text_get_character_count(ATSPI_TEXT(input.get()), nullptr);
    g_assert_cmpint(length, ==, 19);
    text.reset(atspi_text_get_text(ATSPI_TEXT(input.get()), 0, length, nullptr));
    g_assert_cmpstr(text.get(), ==, "Text of input field");

    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(input.get()), 0, ATSPI_TEXT_GRANULARITY_CHAR, nullptr));
    g_assert_cmpstr(range->content, ==, "T");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 1);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(input.get()), 18, ATSPI_TEXT_GRANULARITY_CHAR, nullptr));
    g_assert_cmpstr(range->content, ==, "d");
    g_assert_cmpint(range->start_offset, ==, 18);
    g_assert_cmpint(range->end_offset, ==, 19);

    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(input.get()), 0, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "Text ");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 5);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(input.get()), 16, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "field");
    g_assert_cmpint(range->start_offset, ==, 14);
    g_assert_cmpint(range->end_offset, ==, 19);
}

static void testTextExtents(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow();
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>Text</p>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    auto p = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(p.get()));
    GUniquePtr<AtspiRect> firstCharRect(atspi_text_get_character_extents(ATSPI_TEXT(p.get()), 0, ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_nonnull(firstCharRect.get());
    g_assert_cmpuint(firstCharRect->x, >, 0);
    g_assert_cmpuint(firstCharRect->y, >, 0);
    g_assert_cmpuint(firstCharRect->width, >, 0);
    g_assert_cmpuint(firstCharRect->height, >, 0);
    GUniquePtr<AtspiRect> lastCharRect(atspi_text_get_character_extents(ATSPI_TEXT(p.get()), 3, ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_nonnull(lastCharRect.get());
    g_assert_cmpuint(lastCharRect->x, >, firstCharRect->x);
    g_assert_cmpuint(lastCharRect->y, ==, firstCharRect->y);
    g_assert_cmpuint(lastCharRect->width, >, 0);
    g_assert_cmpuint(lastCharRect->height, >, 0);
    GUniquePtr<AtspiRect> rangeRect(atspi_text_get_range_extents(ATSPI_TEXT(p.get()), 0, 4, ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_nonnull(rangeRect.get());
    g_assert_cmpuint(rangeRect->x, ==, firstCharRect->x);
    g_assert_cmpuint(rangeRect->y, ==, firstCharRect->y);
    g_assert_cmpuint(rangeRect->width, >, 0);
    g_assert_cmpuint(rangeRect->height, >, 0);
#if USE(ATSPI) // Offset at point is broken with ATK.
    auto offset = atspi_text_get_offset_at_point(ATSPI_TEXT(p.get()), firstCharRect->x, firstCharRect->y, ATSPI_COORD_TYPE_WINDOW, nullptr);
    g_assert_cmpint(offset, ==, 0);
    offset = atspi_text_get_offset_at_point(ATSPI_TEXT(p.get()), firstCharRect->x + firstCharRect->width, firstCharRect->y, ATSPI_COORD_TYPE_WINDOW, nullptr);
    g_assert_cmpint(offset, ==, 1);
    offset = atspi_text_get_offset_at_point(ATSPI_TEXT(p.get()), lastCharRect->x, lastCharRect->y, ATSPI_COORD_TYPE_WINDOW, nullptr);
    g_assert_cmpint(offset, ==, 3);
    offset = atspi_text_get_offset_at_point(ATSPI_TEXT(p.get()), lastCharRect->x + lastCharRect->width, lastCharRect->y, ATSPI_COORD_TYPE_WINDOW, nullptr);
    g_assert_cmpint(offset, ==, 4);
#endif
}

static void testTextSelections(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>This is a line of text</p>"
        "    <input value='This is text input value'/>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 2);

    auto p = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(p.get()));

    auto selectionCount = atspi_text_get_n_selections(ATSPI_TEXT(p.get()), nullptr);
    g_assert_cmpuint(selectionCount, ==, 0);
    auto caretOffset = atspi_text_get_caret_offset(ATSPI_TEXT(p.get()), nullptr);
#if USE(ATSPI)
    g_assert_cmpint(caretOffset, ==, -1);
#else
    g_assert_cmpint(caretOffset, ==, 0);
#endif

    GUniquePtr<AtspiRange> selection(atspi_text_get_selection(ATSPI_TEXT(p.get()), 0, nullptr));
    g_assert_nonnull(selection.get());
    g_assert_cmpint(selection->start_offset, ==, 0);
    g_assert_cmpint(selection->end_offset, ==, 0);

    g_assert_true(atspi_text_set_selection(ATSPI_TEXT(p.get()), 0, 5, 14, nullptr));
    selectionCount = atspi_text_get_n_selections(ATSPI_TEXT(p.get()), nullptr);
    g_assert_cmpuint(selectionCount, ==, 1);
    caretOffset = atspi_text_get_caret_offset(ATSPI_TEXT(p.get()), nullptr);
    g_assert_cmpint(caretOffset, ==, 14);
    selection.reset(atspi_text_get_selection(ATSPI_TEXT(p.get()), 0, nullptr));
    g_assert_nonnull(selection.get());
    g_assert_cmpint(selection->start_offset, ==, 5);
    g_assert_cmpint(selection->end_offset, ==, 14);
    GUniquePtr<char> text(atspi_text_get_text(ATSPI_TEXT(p.get()), selection->start_offset, selection->end_offset, nullptr));
    g_assert_cmpstr(text.get(), ==, "is a line");

    g_assert_true(atspi_text_remove_selection(ATSPI_TEXT(p.get()), 0, nullptr));
    selectionCount = atspi_text_get_n_selections(ATSPI_TEXT(p.get()), nullptr);
    g_assert_cmpuint(selectionCount, ==, 0);
    caretOffset = atspi_text_get_caret_offset(ATSPI_TEXT(p.get()), nullptr);
    g_assert_cmpint(caretOffset, ==, 14);
    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(p.get()), 0, nullptr));
    caretOffset = atspi_text_get_caret_offset(ATSPI_TEXT(p.get()), nullptr);
    g_assert_cmpint(caretOffset, ==, 0);

    auto section = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(section.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(section.get(), nullptr), ==, 1);
    auto input = adoptGRef(atspi_accessible_get_child_at_index(section.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(input.get()));
    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(input.get()), 5, nullptr));
    caretOffset = atspi_text_get_caret_offset(ATSPI_TEXT(input.get()), nullptr);
    g_assert_cmpint(caretOffset, ==, 5);
    g_assert_true(atspi_text_set_selection(ATSPI_TEXT(input.get()), 0, 5, 12, nullptr));
    selectionCount = atspi_text_get_n_selections(ATSPI_TEXT(input.get()), nullptr);
    g_assert_cmpuint(selectionCount, ==, 1);
    caretOffset = atspi_text_get_caret_offset(ATSPI_TEXT(input.get()), nullptr);
    g_assert_cmpint(caretOffset, ==, 12);
    selection.reset(atspi_text_get_selection(ATSPI_TEXT(input.get()), 0, nullptr));
    g_assert_nonnull(selection.get());
    g_assert_cmpint(selection->start_offset, ==, 5);
    g_assert_cmpint(selection->end_offset, ==, 12);
    text.reset(atspi_text_get_text(ATSPI_TEXT(input.get()), selection->start_offset, selection->end_offset, nullptr));
    g_assert_cmpstr(text.get(), ==, "is text");
}

static void testTextAttributes(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>This is a <b>line</b> of text</p>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    auto p = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(p.get()));

    // Including default attributes.
    int startOffset, endOffset;
    GRefPtr<GHashTable> attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 0, TRUE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), >, 1);
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "weight")), ==, "400");
    g_assert_cmpint(startOffset, ==, 0);
    g_assert_cmpint(endOffset, ==, 10);

    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 12, TRUE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), >, 1);
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "weight")), ==, "700");
    GUniquePtr<char> value(atspi_text_get_attribute_value(ATSPI_TEXT(p.get()), 11, const_cast<char*>("weight"), nullptr));
    g_assert_cmpstr(value.get(), ==, "700");
    g_assert_cmpint(startOffset, ==, 10);
    g_assert_cmpint(endOffset, ==, 14);

    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 16, TRUE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), >, 1);
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "weight")), ==, "400");
    g_assert_cmpint(startOffset, ==, 14);
    g_assert_cmpint(endOffset, ==, 22);

    // Without default attributes.
    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 0, FALSE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 0);
    g_assert_cmpint(startOffset, ==, 0);
    g_assert_cmpint(endOffset, ==, 10);

    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 12, FALSE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 1);
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "weight")), ==, "700");
    g_assert_cmpint(startOffset, ==, 10);
    g_assert_cmpint(endOffset, ==, 14);

    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 16, FALSE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 0);
    g_assert_cmpint(startOffset, ==, 14);
    g_assert_cmpint(endOffset, ==, 22);

    // Only default attributes.
    attributes = adoptGRef(atspi_text_get_default_attributes(ATSPI_TEXT(p.get()), nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), >, 1);
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "weight")), ==, "400");

#if USE(ATSPI)
    // Atspi implementation handles ranges with the same attributes as one run.
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>This is a <b>li</b><b>ne</b> of text</p>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    p = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(p.get()));

    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 0, TRUE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), >, 1);
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "weight")), ==, "400");
    g_assert_cmpint(startOffset, ==, 0);
    g_assert_cmpint(endOffset, ==, 10);

    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 12, TRUE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), >, 1);
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "weight")), ==, "700");
    g_assert_cmpint(startOffset, ==, 10);
    g_assert_cmpint(endOffset, ==, 14);

    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 16, TRUE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), >, 1);
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "weight")), ==, "400");
    g_assert_cmpint(startOffset, ==, 14);
    g_assert_cmpint(endOffset, ==, 22);

#endif
}

static void testTextStateChanged(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>This is a line of text</p>"
        "    <input value='This is a text field'/>"
        "    <div contenteditable=true>This is content editable</div>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 3);

    // Text caret moved.
    auto p = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(p.get()));
    test->startEventMonitor(p.get(), { "object:text-caret-moved" });
    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(p.get()), 10, nullptr));
    auto events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-caret-moved");
    g_assert_cmpuint(events[0]->detail1, ==, 10);

    auto section = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(section.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(section.get(), nullptr), ==, 1);
    auto input = adoptGRef(atspi_accessible_get_child_at_index(section.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(input.get()));
    test->startEventMonitor(input.get(), { "object:text-caret-moved" });
    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(input.get()), 5, nullptr));
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-caret-moved");
    g_assert_cmpuint(events[0]->detail1, ==, 5);

    auto div = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 2, nullptr));
    g_assert_true(ATSPI_IS_TEXT(div.get()));
    test->startEventMonitor(div.get(), { "object:text-caret-moved" });
    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(div.get()), 15, nullptr));
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-caret-moved");
    g_assert_cmpuint(events[0]->detail1, ==, 15);

#if USE(ATSPI) // Selection changed seems to be broken with ATK.
    // Selection changed.
    test->startEventMonitor(p.get(), { "object:text-selection-changed", "object:text-caret-moved" });
    g_assert_true(atspi_text_set_selection(ATSPI_TEXT(p.get()), 0, 10, 15, nullptr));
    events = test->stopEventMonitor(2);
    g_assert_cmpuint(events.size(), ==, 2);
    auto* event = AccessibilityTest::findEvent(events, "object:text-caret-moved");
    g_assert_nonnull(event);
    g_assert_cmpuint(event->detail1, ==, 15);
    g_assert_nonnull(AccessibilityTest::findEvent(events, "object:text-selection-changed"));

    test->startEventMonitor(input.get(), { "object:text-selection-changed", "object:text-caret-moved" });
    g_assert_true(atspi_text_set_selection(ATSPI_TEXT(input.get()), 0, 5, 10, nullptr));
    events = test->stopEventMonitor(2);
    g_assert_cmpuint(events.size(), ==, 2);
    event = AccessibilityTest::findEvent(events, "object:text-caret-moved");
    g_assert_nonnull(event);
    g_assert_cmpuint(event->detail1, ==, 10);
    g_assert_nonnull(AccessibilityTest::findEvent(events, "object:text-selection-changed"));

    test->startEventMonitor(div.get(), { "object:text-selection-changed", "object:text-caret-moved" });
    g_assert_true(atspi_text_set_selection(ATSPI_TEXT(div.get()), 0, 15, 18, nullptr));
    events = test->stopEventMonitor(2);
    g_assert_cmpuint(events.size(), ==, 2);
    event = AccessibilityTest::findEvent(events, "object:text-caret-moved");
    g_assert_nonnull(event);
    g_assert_cmpuint(event->detail1, ==, 18);
    g_assert_nonnull(AccessibilityTest::findEvent(events, "object:text-selection-changed"));
#endif

    // Text changed.
    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(input.get()), 5, nullptr));
    test->startEventMonitor(input.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->keyStroke(GDK_KEY_a);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:insert");
#if USE(ATSPI)
    g_assert_cmpuint(events[0]->detail1, ==, 5);
#endif
    g_assert_cmpuint(events[0]->detail2, ==, 1);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "a");
    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(input.get()), 0, nullptr));
    test->startEventMonitor(input.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->keyStroke(GDK_KEY_Delete);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:delete");
    g_assert_cmpuint(events[0]->detail1, ==, 0);
    g_assert_cmpuint(events[0]->detail2, ==, 1);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "T");

    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(div.get()), 10, nullptr));
    test->startEventMonitor(div.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->keyStroke(GDK_KEY_b);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:insert");
#if USE(ATSPI)
    g_assert_cmpuint(events[0]->detail1, ==, 10);
#endif
    g_assert_cmpuint(events[0]->detail2, ==, 1);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "b");
    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(div.get()), 15, nullptr));
    test->startEventMonitor(div.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->keyStroke(GDK_KEY_BackSpace);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:delete");
#if USE(ATSPI)
    g_assert_cmpuint(events[0]->detail1, ==, 14);
#endif
    g_assert_cmpuint(events[0]->detail2, ==, 1);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "n");

    // Text replaced.
    g_assert_true(atspi_text_set_selection(ATSPI_TEXT(input.get()), 0, 5, 10, nullptr));
    test->startEventMonitor(input.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->keyStroke(GDK_KEY_c);
    events = test->stopEventMonitor(2);
    g_assert_cmpuint(events.size(), ==, 2);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:delete");
    g_assert_cmpuint(events[0]->detail1, ==, 6);
    g_assert_cmpuint(events[0]->detail2, ==, 5);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "is a ");
    g_assert_cmpstr(events[1]->type, ==, "object:text-changed:insert");
#if USE(ATSPI)
    g_assert_cmpuint(events[1]->detail1, ==, 5);
#endif
    g_assert_cmpuint(events[1]->detail2, ==, 1);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[1]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[1]->any_data), ==, "c");

    g_assert_true(atspi_text_set_selection(ATSPI_TEXT(div.get()), 0, 10, 18, nullptr));
    test->startEventMonitor(div.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->keyStroke(GDK_KEY_Delete);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:delete");
#if USE(ATSPI)
    g_assert_cmpuint(events[0]->detail1, ==, 10);
#endif
    g_assert_cmpuint(events[0]->detail2, ==, 8);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "bntet ed");

#if USE(ATSPI)
    // Text input value changed.
    test->startEventMonitor(input.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->runJavaScriptAndWaitUntilFinished("document.getElementsByTagName('input')[0].value = 'foo';", nullptr);
    events = test->stopEventMonitor(2);
    g_assert_cmpuint(events.size(), ==, 2);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:delete");
    g_assert_cmpuint(events[0]->detail1, ==, 0);
    g_assert_cmpuint(events[0]->detail2, ==, 16);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "his actext field");
    g_assert_cmpstr(events[1]->type, ==, "object:text-changed:insert");
    g_assert_cmpuint(events[1]->detail1, ==, 0);
    g_assert_cmpuint(events[1]->detail2, ==, 3);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[1]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[1]->any_data), ==, "foo");
#endif
}

void beforeAll()
{
    AccessibilityTest::add("WebKitAccessibility", "accessible/basic-hierarchy", testAccessibleBasicHierarchy);
    AccessibilityTest::add("WebKitAccessibility", "accessible/ignored-objects", testAccessibleIgnoredObjects);
    AccessibilityTest::add("WebKitAccessibility", "accessible/children-changed", testAccessibleChildrenChanged);
    AccessibilityTest::add("WebKitAccessibility", "accessible/attributes", testAccessibleAttributes);
    AccessibilityTest::add("WebKitAccessibility", "accessible/state", testAccessibleState);
    AccessibilityTest::add("WebKitAccessibility", "accessible/state-changed", testAccessibleStateChanged);
    AccessibilityTest::add("WebKitAccessibility", "component/hit-test", testComponentHitTest);
#ifdef ATSPI_SCROLLTYPE_COUNT
    AccessibilityTest::add("WebKitAccessibility", "component/scroll-to", testComponentScrollTo);
#endif
    AccessibilityTest::add("WebKitAccessibility", "text/basic", testTextBasic);
    AccessibilityTest::add("WebKitAccessibility", "text/surrogate-pair", testTextSurrogatePair);
    AccessibilityTest::add("WebKitAccessibility", "text/iterator", testTextIterator);
    AccessibilityTest::add("WebKitAccessibility", "text/extents", testTextExtents);
    AccessibilityTest::add("WebKitAccessibility", "text/selections", testTextSelections);
    AccessibilityTest::add("WebKitAccessibility", "text/attributes", testTextAttributes);
    AccessibilityTest::add("WebKitAccessibility", "text/state-changed", testTextStateChanged);
}

void afterAll()
{
}
