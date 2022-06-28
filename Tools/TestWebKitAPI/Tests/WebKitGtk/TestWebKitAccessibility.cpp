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
        for (int i = childCount - 1; i >= 0; --i)  {
            GRefPtr<AtspiAccessible> current = adoptGRef(atspi_accessible_get_child_at_index(desktop.get(), i, nullptr));
            GUniquePtr<char> name(atspi_accessible_get_name(current.get(), nullptr));
            if (!g_strcmp0(name.get(), "TestWebKitAccessibility"))
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

    static bool accessibleApplicationIsTestProgram(AtspiAccessible* accessible)
    {
        GRefPtr<AtspiAccessible> application = adoptGRef(atspi_accessible_get_application(accessible, nullptr));
        GUniquePtr<char> applicationName(atspi_accessible_get_name(application.get(), nullptr));
        return !g_strcmp0(applicationName.get(), "TestWebKitAccessibility");
    }

    bool shouldProcessEvent(AtspiEvent* event)
    {
        // source = std::nullopt -> no filter.
        if (!m_eventMonitor.source)
            return true;

        // source = nullptr -> filter by application.
        if (!m_eventMonitor.source.value())
            return accessibleApplicationIsTestProgram(event->source);

        // source != nullptr -> filter by accessible.
        return m_eventMonitor.source.value() == event->source;
    }

    void startEventMonitor(std::optional<AtspiAccessible*> source, Vector<CString>&& events)
    {
        m_eventMonitor.source = source;
        m_eventMonitor.eventTypes = WTFMove(events);
        m_eventMonitor.listener = adoptGRef(atspi_event_listener_new([](AtspiEvent* event, gpointer userData) {
            auto* test = static_cast<AccessibilityTest*>(userData);
            if (test->shouldProcessEvent(event))
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

    static bool isSelected(AtspiAccessible* element)
    {
        GRefPtr<AtspiStateSet> set = adoptGRef(atspi_accessible_get_state_set(element));
        return atspi_state_set_contains(set.get(), ATSPI_STATE_SELECTED);
    }

private:
    AtspiAccessible* m_eventSource { nullptr };

    struct {
        GRefPtr<AtspiEventListener> listener;
        Vector<CString> eventTypes;
        Vector<UniqueAtspiEvent> events;
        std::optional<AtspiAccessible*> source;
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
    // Check that children-changed::remove is emitted on the root object on navigation,
    // and the a11y hierarchy is updated.
    test->waitUntilChildrenRemoved(rootObject.get());

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

    static const char* toolkitName = "WebKitGTK";

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 2);
    auto attributes = adoptGRef(atspi_accessible_get_attributes(documentWeb.get(), nullptr));
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 1);
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
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "id")), ==, "webkitgtk");
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
        "    <ul><li aria-current='step'>Current</li></ul>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 3);

    auto h1 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(h1.get()));
    GRefPtr<AtspiStateSet> stateSet = adoptGRef(atspi_accessible_get_state_set(h1.get()));
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 4);
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
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 5);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_ENABLED));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SENSITIVE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_FOCUSABLE));

    auto buttonDisabled = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(buttonDisabled.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(buttonDisabled.get()));
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 2);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));

    auto toggleButton = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(toggleButton.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(toggleButton.get()));
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 6);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_ENABLED));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SENSITIVE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_FOCUSABLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_PRESSED));

    auto radio = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(radio.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(radio.get()));
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 6);
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
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SELECTABLE_TEXT));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_REQUIRED));

    auto entryDisabled = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(entryDisabled.get()));
    stateSet = adoptGRef(atspi_accessible_get_state_set(entryDisabled.get()));
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 5);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_EDITABLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SINGLE_LINE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SELECTABLE_TEXT));

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
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SELECTABLE_TEXT));
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
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SELECTABLE_TEXT));

    auto ul = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 2, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(textArea.get()));
    auto li = adoptGRef(atspi_accessible_get_child_at_index(ul.get(), 0, nullptr));
    stateSet = adoptGRef(atspi_accessible_get_state_set(li.get()));
    g_assert_cmpuint(AccessibilityTest::stateSetSize(stateSet.get()), ==, 5);
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_ACTIVE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_ENABLED));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SENSITIVE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_VISIBLE));
    g_assert_true(atspi_state_set_contains(stateSet.get(), ATSPI_STATE_SHOWING));
}

static void testAccessibleStateChanged(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <input id='check' type='checkbox'/>Checkbox"
        "    <button id='toggle' aria-pressed='true'>Toggle button</button>"
        "    <input id='entry' value='Entry'>"
        "    <select id='list' size='2'><option>1</option><option>2</option></select>"
        "    <select id='combo'><option>1</option><option>2</option></select>"
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
    g_assert_cmpint(atspi_accessible_get_child_count(section.get(), nullptr), ==, 5);

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

    test->runJavaScriptAndWaitUntilFinished("document.getElementById('list').focus();", nullptr);
    auto listBox = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(listBox.get()));
    auto option1 = adoptGRef(atspi_accessible_get_child_at_index(listBox.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(option1.get()));
    auto option2 = adoptGRef(atspi_accessible_get_child_at_index(listBox.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(option2.get()));
    g_assert_false(AccessibilityTest::isSelected(option1.get()));
    g_assert_false(AccessibilityTest::isSelected(option2.get()));
    test->startEventMonitor(option1.get(), { "object:state-changed" });
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('list').selectedIndex = 0;", nullptr);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:state-changed:selected");
    g_assert_cmpuint(events[0]->detail1, ==, 1);
    events = { };
    g_assert_true(AccessibilityTest::isSelected(option1.get()));
    g_assert_false(AccessibilityTest::isSelected(option2.get()));
    test->startEventMonitor(option1.get(), { "object:state-changed" });
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('list').selectedIndex = 1;", nullptr);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:state-changed:selected");
    g_assert_cmpuint(events[0]->detail1, ==, 0);
    events = { };
    g_assert_false(AccessibilityTest::isSelected(option1.get()));
    g_assert_true(AccessibilityTest::isSelected(option2.get()));

    test->runJavaScriptAndWaitUntilFinished("document.getElementById('combo').focus();", nullptr);
    auto combo = adoptGRef(atspi_accessible_get_child_at_index(section.get(), nextChild++, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(combo.get()));
    auto menuList = adoptGRef(atspi_accessible_get_child_at_index(combo.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(menuList.get()));
    option1 = adoptGRef(atspi_accessible_get_child_at_index(menuList.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(option1.get()));
    option2 = adoptGRef(atspi_accessible_get_child_at_index(menuList.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(option2.get()));
    g_assert_true(AccessibilityTest::isSelected(option1.get()));
    g_assert_false(AccessibilityTest::isSelected(option2.get()));
    test->startEventMonitor(option2.get(), { "object:state-changed" });
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('combo').selectedIndex = 1;", nullptr);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:state-changed:selected");
    g_assert_cmpuint(events[0]->detail1, ==, 1);
    events = { };
    g_assert_true(AccessibilityTest::isSelected(option2.get()));
    g_assert_false(AccessibilityTest::isSelected(option1.get()));
    test->startEventMonitor(option2.get(), { "object:state-changed" });
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('combo').selectedIndex = 0", nullptr);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:state-changed:selected");
    g_assert_cmpuint(events[0]->detail1, ==, 0);
    events = { };
    g_assert_true(AccessibilityTest::isSelected(option1.get()));
    g_assert_false(AccessibilityTest::isSelected(option2.get()));
}

static void testAccessibleEventListener(AccessibilityTest* test, gconstpointer)
{
    test->startEventMonitor(nullptr, { "object:state-changed:focused" });
    test->showInWindow();
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <input id='entry' type='text'/>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    test->runJavaScriptAndWaitUntilFinished("document.getElementById('entry').focus();", nullptr);

    auto events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:state-changed:focused");
    auto* entry = events[0]->source;
    g_assert_true(ATSPI_IS_ACCESSIBLE(entry));
    g_assert_cmpint(atspi_accessible_get_role(entry, nullptr), ==, ATSPI_ROLE_ENTRY);

    auto panel = adoptGRef(atspi_accessible_get_parent(entry, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(panel.get()));
    g_assert_cmpint(atspi_accessible_get_role(panel.get(), nullptr), ==, ATSPI_ROLE_PANEL);

    auto documentWeb = adoptGRef(atspi_accessible_get_parent(panel.get(), nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_role(documentWeb.get(), nullptr), ==, ATSPI_ROLE_DOCUMENT_WEB);

    auto scrollView = adoptGRef(atspi_accessible_get_parent(documentWeb.get(), nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(scrollView.get()));
    g_assert_cmpint(atspi_accessible_get_role(scrollView.get(), nullptr), ==, ATSPI_ROLE_SCROLL_PANE);

    auto rootObject = adoptGRef(atspi_accessible_get_parent(scrollView.get(), nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(rootObject.get()));
    g_assert_cmpint(atspi_accessible_get_role(rootObject.get(), nullptr), ==, ATSPI_ROLE_FILLER);
}

static void testAccessibleListMarkers(AccessibilityTest* test, gconstpointer)
{
    GUniquePtr<char> baseDir(g_strdup_printf("file://%s/", Test::getResourcesDir().data()));
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <head>"
        "    <style>"
        "      .img { list-style-image: url(blank.ico) }"
        "    </style>"
        "  </head>"
        "  <body>"
        "    <ol>"
        "      <li>List item 1</li>"
        "    </ol>"
        "    <ul class='img'>"
        "      <li>List item 1</li>"
        "    </ul>"
        "  </body>"
        "</html>",
        baseDir.get());
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 2);

    auto ol = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(ol.get()));
    g_assert_cmpint(atspi_accessible_get_role(ol.get(), nullptr), ==, ATSPI_ROLE_LIST);
    g_assert_cmpint(atspi_accessible_get_child_count(ol.get(), nullptr), ==, 1);

    auto li = adoptGRef(atspi_accessible_get_child_at_index(ol.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(li.get()));
    g_assert_cmpint(atspi_accessible_get_role(li.get(), nullptr), ==, ATSPI_ROLE_LIST_ITEM);
    g_assert_cmpint(atspi_accessible_get_child_count(li.get(), nullptr), ==, 1);
    auto marker = adoptGRef(atspi_accessible_get_child_at_index(li.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(marker.get()));
    g_assert_cmpint(atspi_accessible_get_role(marker.get(), nullptr), ==, ATSPI_ROLE_TEXT);
    GUniquePtr<char> name(atspi_accessible_get_role_name(marker.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "text");
    name.reset(atspi_accessible_get_localized_role_name(marker.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "text");
    GRefPtr<AtspiText> text = adoptGRef(atspi_accessible_get_text_iface(marker.get()));
    g_assert_nonnull(text.get());

    auto ul = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(ul.get()));
    g_assert_cmpint(atspi_accessible_get_role(ul.get(), nullptr), ==, ATSPI_ROLE_LIST);
    g_assert_cmpint(atspi_accessible_get_child_count(ul.get(), nullptr), ==, 1);

    li = adoptGRef(atspi_accessible_get_child_at_index(ul.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(li.get()));
    g_assert_cmpint(atspi_accessible_get_role(li.get(), nullptr), ==, ATSPI_ROLE_LIST_ITEM);
    g_assert_cmpint(atspi_accessible_get_child_count(li.get(), nullptr), ==, 1);
    marker = adoptGRef(atspi_accessible_get_child_at_index(li.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(marker.get()));
    g_assert_cmpint(atspi_accessible_get_role(marker.get(), nullptr), ==, ATSPI_ROLE_IMAGE);
    name.reset(atspi_accessible_get_role_name(marker.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "image");
    name.reset(atspi_accessible_get_localized_role_name(marker.get(), nullptr));
    g_assert_cmpstr(name.get(), ==, "image");
    GRefPtr<AtspiImage> image = adoptGRef(atspi_accessible_get_image_iface(marker.get()));
    g_assert_nonnull(image.get());
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
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 0);
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

    // Paragraph granularity.
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 0, ATSPI_TEXT_GRANULARITY_PARAGRAPH, nullptr));
    g_assert_cmpstr(range->content, ==, "Text of first sentence. This is the second sentence.");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 52);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 83, ATSPI_TEXT_GRANULARITY_PARAGRAPH, nullptr));
    g_assert_cmpstr(range->content, ==, "And this is the next paragraph.");
    g_assert_cmpint(range->start_offset, ==, 53);
    g_assert_cmpint(range->end_offset, ==, 84);

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

    // Password field now.
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <input type='password' value='password value'/>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    section = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(section.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(section.get(), nullptr), ==, 1);
    input = adoptGRef(atspi_accessible_get_child_at_index(section.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(input.get()));
    length = atspi_text_get_character_count(ATSPI_TEXT(input.get()), nullptr);
    g_assert_cmpint(length, ==, 14);
    text.reset(atspi_text_get_text(ATSPI_TEXT(input.get()), 0, length, nullptr));
    g_assert_cmpstr(text.get(), ==, "••••••••••••••");

    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(input.get()), 0, ATSPI_TEXT_GRANULARITY_CHAR, nullptr));
    g_assert_cmpstr(range->content, ==, "•");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 1);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(input.get()), 13, ATSPI_TEXT_GRANULARITY_CHAR, nullptr));
    g_assert_cmpstr(range->content, ==, "•");
    g_assert_cmpint(range->start_offset, ==, 13);
    g_assert_cmpint(range->end_offset, ==, 14);

    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(input.get()), 0, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "••••••••••••••");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 14);
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

    auto offset = atspi_text_get_offset_at_point(ATSPI_TEXT(p.get()), firstCharRect->x, firstCharRect->y, ATSPI_COORD_TYPE_WINDOW, nullptr);
    g_assert_cmpint(offset, ==, 0);
    offset = atspi_text_get_offset_at_point(ATSPI_TEXT(p.get()), firstCharRect->x + firstCharRect->width, firstCharRect->y, ATSPI_COORD_TYPE_WINDOW, nullptr);
    g_assert_cmpint(offset, ==, 1);
    offset = atspi_text_get_offset_at_point(ATSPI_TEXT(p.get()), lastCharRect->x, lastCharRect->y, ATSPI_COORD_TYPE_WINDOW, nullptr);
    g_assert_cmpint(offset, ==, 3);
    offset = atspi_text_get_offset_at_point(ATSPI_TEXT(p.get()), lastCharRect->x + lastCharRect->width, lastCharRect->y, ATSPI_COORD_TYPE_WINDOW, nullptr);
    g_assert_cmpint(offset, ==, 4);
}

static void testTextSelections(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>This is a line of text</p>"
        "    <input value='This is text input value'/>"
        "    <input type='password' value='secret'/>"
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
    g_assert_cmpint(caretOffset, ==, -1);

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
    g_assert_cmpint(atspi_accessible_get_child_count(section.get(), nullptr), ==, 2);
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

    auto password = adoptGRef(atspi_accessible_get_child_at_index(section.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_TEXT(password.get()));
    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(password.get()), 2, nullptr));
    caretOffset = atspi_text_get_caret_offset(ATSPI_TEXT(password.get()), nullptr);
    g_assert_cmpint(caretOffset, ==, 2);
    g_assert_true(atspi_text_set_selection(ATSPI_TEXT(password.get()), 0, 2, 5, nullptr));
    selectionCount = atspi_text_get_n_selections(ATSPI_TEXT(password.get()), nullptr);
    g_assert_cmpuint(selectionCount, ==, 1);
    caretOffset = atspi_text_get_caret_offset(ATSPI_TEXT(password.get()), nullptr);
    g_assert_cmpint(caretOffset, ==, 5);
    selection.reset(atspi_text_get_selection(ATSPI_TEXT(password.get()), 0, nullptr));
    g_assert_nonnull(selection.get());
    g_assert_cmpint(selection->start_offset, ==, 2);
    g_assert_cmpint(selection->end_offset, ==, 5);
    text.reset(atspi_text_get_text(ATSPI_TEXT(password.get()), selection->start_offset, selection->end_offset, nullptr));
    g_assert_cmpstr(text.get(), ==, "•••");
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
        "    <input type='password' value='123'/>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 4);

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

    section = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 3, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(section.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(section.get(), nullptr), ==, 1);
    auto password = adoptGRef(atspi_accessible_get_child_at_index(section.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(password.get()));
    test->startEventMonitor(password.get(), { "object:text-caret-moved" });
    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(password.get()), 2, nullptr));
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-caret-moved");
    g_assert_cmpuint(events[0]->detail1, ==, 2);

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

    test->startEventMonitor(password.get(), { "object:text-selection-changed", "object:text-caret-moved" });
    g_assert_true(atspi_text_set_selection(ATSPI_TEXT(password.get()), 0, 2, 3, nullptr));
    events = test->stopEventMonitor(2);
    g_assert_cmpuint(events.size(), ==, 2);
    event = AccessibilityTest::findEvent(events, "object:text-caret-moved");
    g_assert_nonnull(event);
    g_assert_cmpuint(event->detail1, ==, 3);
    g_assert_nonnull(AccessibilityTest::findEvent(events, "object:text-selection-changed"));

    // Text changed.
    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(input.get()), 5, nullptr));
    test->startEventMonitor(input.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->keyStroke(GDK_KEY_a);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:insert");
    g_assert_cmpuint(events[0]->detail1, ==, 5);
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
    g_assert_cmpuint(events[0]->detail1, ==, 10);
    g_assert_cmpuint(events[0]->detail2, ==, 1);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "b");
    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(div.get()), 15, nullptr));
    test->startEventMonitor(div.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->keyStroke(GDK_KEY_BackSpace);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:delete");
    g_assert_cmpuint(events[0]->detail1, ==, 14);
    g_assert_cmpuint(events[0]->detail2, ==, 1);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "n");

    g_assert_true(atspi_text_set_caret_offset(ATSPI_TEXT(password.get()), 2, nullptr));
    test->startEventMonitor(password.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->keyStroke(GDK_KEY_a);
    events = test->stopEventMonitor(1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:insert");
    g_assert_cmpuint(events[0]->detail1, ==, 2);
    g_assert_cmpuint(events[0]->detail2, ==, 1);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "•");
    test->startEventMonitor(password.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->keyStroke(GDK_KEY_BackSpace);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:delete");
    g_assert_cmpuint(events[0]->detail1, ==, 2);
    g_assert_cmpuint(events[0]->detail2, ==, 1);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "•");

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
    g_assert_cmpuint(events[1]->detail1, ==, 5);
    g_assert_cmpuint(events[1]->detail2, ==, 1);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[1]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[1]->any_data), ==, "c");

    g_assert_true(atspi_text_set_selection(ATSPI_TEXT(div.get()), 0, 10, 18, nullptr));
    test->startEventMonitor(div.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->keyStroke(GDK_KEY_Delete);
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:delete");
    g_assert_cmpuint(events[0]->detail1, ==, 10);
    g_assert_cmpuint(events[0]->detail2, ==, 8);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "bntet ed");

    g_assert_true(atspi_text_set_selection(ATSPI_TEXT(password.get()), 0, 0, 3, nullptr));
    test->startEventMonitor(password.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->keyStroke(GDK_KEY_z);
    events = test->stopEventMonitor(2);
    g_assert_cmpuint(events.size(), ==, 2);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:delete");
    g_assert_cmpuint(events[0]->detail1, ==, 1);
    g_assert_cmpuint(events[0]->detail2, ==, 3);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "•••");
    g_assert_cmpstr(events[1]->type, ==, "object:text-changed:insert");
    g_assert_cmpuint(events[1]->detail1, ==, 0);
    g_assert_cmpuint(events[1]->detail2, ==, 1);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[1]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[1]->any_data), ==, "•");

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

    test->startEventMonitor(password.get(), { "object:text-changed:insert", "object:text-changed:delete" });
    test->runJavaScriptAndWaitUntilFinished("document.getElementsByTagName('input')[1].value = '7890';", nullptr);
    events = test->stopEventMonitor(2);
    g_assert_cmpuint(events.size(), ==, 2);
    g_assert_cmpstr(events[0]->type, ==, "object:text-changed:delete");
    g_assert_cmpuint(events[0]->detail1, ==, 0);
    g_assert_cmpuint(events[0]->detail2, ==, 1);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[0]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[0]->any_data), ==, "•");
    g_assert_cmpstr(events[1]->type, ==, "object:text-changed:insert");
    g_assert_cmpuint(events[1]->detail1, ==, 0);
    g_assert_cmpuint(events[1]->detail2, ==, 4);
    g_assert_true(G_VALUE_HOLDS_STRING(&events[1]->any_data));
    g_assert_cmpstr(g_value_get_string(&events[1]->any_data), ==, "••••");
}

static void testTextReplacedObjects(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>This is <button>button1</button> and <button>button2</button> in paragraph</p>"
        "    <p>This is <a href='#'>link1</a> and <a href='#'>link2</a> in paragraph</p>"
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
    g_assert_cmpint(length, ==, 28);
    GUniquePtr<char> text(atspi_text_get_text(ATSPI_TEXT(p.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "This is \357\277\274 and \357\277\274 in paragraph");
    g_assert_cmpint(atspi_accessible_get_child_count(p.get(), nullptr), ==, 2);

    auto button1 = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(button1.get()));
    text.reset(atspi_accessible_get_name(button1.get(), nullptr));
    g_assert_cmpstr(text.get(), ==, "button1");

    auto button2 = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(button2.get()));
    text.reset(atspi_accessible_get_name(button2.get(), nullptr));
    g_assert_cmpstr(text.get(), ==, "button2");

    UniqueAtspiTextRange range(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 8, ATSPI_TEXT_GRANULARITY_CHAR, nullptr));
    g_assert_cmpstr(range->content, ==, "\357\277\274");
    g_assert_cmpint(range->start_offset, ==, 8);
    g_assert_cmpint(range->end_offset, ==, 9);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 14, ATSPI_TEXT_GRANULARITY_CHAR, nullptr));
    g_assert_cmpstr(range->content, ==, "\357\277\274");
    g_assert_cmpint(range->start_offset, ==, 14);
    g_assert_cmpint(range->end_offset, ==, 15);

    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 8, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "\357\277\274 ");
    g_assert_cmpint(range->start_offset, ==, 8);
    g_assert_cmpint(range->end_offset, ==, 10);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(p.get()), 14, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "\357\277\274 ");
    g_assert_cmpint(range->start_offset, ==, 14);
    g_assert_cmpint(range->end_offset, ==, 16);

    int startOffset, endOffset;
    GRefPtr<GHashTable> attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 0, FALSE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 0);
    g_assert_cmpint(startOffset, ==, 0);
    g_assert_cmpint(endOffset, ==, 8);

    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 9, FALSE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), >, 0);
    g_assert_cmpint(startOffset, ==, 8);
    g_assert_cmpint(endOffset, ==, 9);

    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 11, FALSE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 0);
    g_assert_cmpint(startOffset, ==, 9);
    g_assert_cmpint(endOffset, ==, 14);

    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 15, FALSE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), >, 0);
    g_assert_cmpint(startOffset, ==, 14);
    g_assert_cmpint(endOffset, ==, 15);

    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(p.get()), 18, FALSE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 0);
    g_assert_cmpint(startOffset, ==, 15);
    g_assert_cmpint(endOffset, ==, 28);

    // Links are also replaced elements.
    p = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_TEXT(p.get()));
    g_assert_cmpint(atspi_text_get_character_count(ATSPI_TEXT(p.get()), nullptr), ==, 28);
    text.reset(atspi_text_get_text(ATSPI_TEXT(p.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "This is \357\277\274 and \357\277\274 in paragraph");
    g_assert_cmpint(atspi_accessible_get_child_count(p.get(), nullptr), ==, 2);

    auto link1 = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(link1.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(link1.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "link1");

    auto link2 = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_TEXT(link2.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(link2.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "link2");
}

static void testTextListMarkers(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <ul>"
        "      <li>List <b>item</b> 1</li>"
        "    </ul>"
        "    <ol style='direction:rtl;'>"
        "      <li>List <b>item</b> 1</li>"
        "    </ol>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 2);

    auto ul = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(ul.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(ul.get(), nullptr), ==, 1);

    auto li = adoptGRef(atspi_accessible_get_child_at_index(ul.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(li.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(li.get(), nullptr), ==, 1);

    auto length = atspi_text_get_character_count(ATSPI_TEXT(li.get()), nullptr);
    g_assert_cmpint(length, ==, 12);

    GUniquePtr<char> text(atspi_text_get_text(ATSPI_TEXT(li.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "\357\277\274List item 1");

    UniqueAtspiTextRange range(atspi_text_get_string_at_offset(ATSPI_TEXT(li.get()), 0, ATSPI_TEXT_GRANULARITY_CHAR, nullptr));
    g_assert_cmpstr(range->content, ==, "\357\277\274");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 1);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(li.get()), 1, ATSPI_TEXT_GRANULARITY_CHAR, nullptr));
    g_assert_cmpstr(range->content, ==, "L");
    g_assert_cmpint(range->start_offset, ==, 1);
    g_assert_cmpint(range->end_offset, ==, 2);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(li.get()), 0, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "\357\277\274");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 1);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(li.get()), 1, ATSPI_TEXT_GRANULARITY_WORD, nullptr));
    g_assert_cmpstr(range->content, ==, "List ");
    g_assert_cmpint(range->start_offset, ==, 1);
    g_assert_cmpint(range->end_offset, ==, 6);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(li.get()), 0, ATSPI_TEXT_GRANULARITY_LINE, nullptr));
    g_assert_cmpstr(range->content, ==, "\357\277\274List item 1");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 12);
    range.reset(atspi_text_get_string_at_offset(ATSPI_TEXT(li.get()), 1, ATSPI_TEXT_GRANULARITY_LINE, nullptr));
    g_assert_cmpstr(range->content, ==, "\357\277\274List item 1");
    g_assert_cmpint(range->start_offset, ==, 0);
    g_assert_cmpint(range->end_offset, ==, 12);

    int startOffset, endOffset;
    GRefPtr<GHashTable> attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(li.get()), 0, FALSE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 0);
    g_assert_cmpint(startOffset, ==, 0);
    g_assert_cmpint(endOffset, ==, 1);
    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(li.get()), 3, FALSE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 0);
    g_assert_cmpint(startOffset, ==, 1);
    g_assert_cmpint(endOffset, ==, 6);
    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(li.get()), 8, FALSE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), >, 0);
    g_assert_cmpint(startOffset, ==, 6);
    g_assert_cmpint(endOffset, ==, 10);
    attributes = adoptGRef(atspi_text_get_attribute_run(ATSPI_TEXT(li.get()), 11, FALSE, &startOffset, &endOffset, nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 0);
    g_assert_cmpint(startOffset, ==, 10);
    g_assert_cmpint(endOffset, ==, 12);

    auto marker = adoptGRef(atspi_accessible_get_child_at_index(li.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(marker.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(marker.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "• ");

    auto ol = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(ol.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(ol.get(), nullptr), ==, 1);

    li = adoptGRef(atspi_accessible_get_child_at_index(ol.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(li.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(li.get(), nullptr), ==, 1);

    length = atspi_text_get_character_count(ATSPI_TEXT(li.get()), nullptr);
    g_assert_cmpint(length, ==, 12);

    text.reset(atspi_text_get_text(ATSPI_TEXT(li.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "List item 1\357\277\274");

    marker = adoptGRef(atspi_accessible_get_child_at_index(li.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TEXT(marker.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(marker.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "1. ");
}

static void testValueBasic(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <input type='range' min='0' max='100' value='50' step='25'/>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    auto panel = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(panel.get()));
    g_assert_cmpint(atspi_accessible_get_role(panel.get(), nullptr), ==, ATSPI_ROLE_PANEL);

    auto slider = adoptGRef(atspi_accessible_get_child_at_index(panel.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_VALUE(slider.get()));
    g_assert_cmpfloat(atspi_value_get_current_value(ATSPI_VALUE(slider.get()), nullptr), ==, 50);
    g_assert_cmpfloat(atspi_value_get_minimum_value(ATSPI_VALUE(slider.get()), nullptr), ==, 0);
    g_assert_cmpfloat(atspi_value_get_maximum_value(ATSPI_VALUE(slider.get()), nullptr), ==, 100);
    g_assert_cmpfloat(atspi_value_get_minimum_increment(ATSPI_VALUE(slider.get()), nullptr), ==, 25);

    test->startEventMonitor(slider.get(), { "object:property-change:accessible-value" });
    g_assert_true(atspi_value_set_current_value(ATSPI_VALUE(slider.get()), 75, nullptr));
    auto events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:property-change:accessible-value");
    g_assert_cmpfloat(atspi_value_get_current_value(ATSPI_VALUE(slider.get()), nullptr), ==, 75);

    test->startEventMonitor(slider.get(), { "object:property-change:accessible-value" });
    g_assert_true(atspi_value_set_current_value(ATSPI_VALUE(slider.get()), 125, nullptr));
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:property-change:accessible-value");
    g_assert_cmpfloat(atspi_value_get_current_value(ATSPI_VALUE(slider.get()), nullptr), ==, 100);
    test->startEventMonitor(slider.get(), { "object:property-change:accessible-value" });
    g_assert_true(atspi_value_set_current_value(ATSPI_VALUE(slider.get()), -25, nullptr));
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:property-change:accessible-value");
    g_assert_cmpfloat(atspi_value_get_current_value(ATSPI_VALUE(slider.get()), nullptr), ==, 0);
}

static void testHyperlinkBasic(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <a href='https://www.webkitgtk.org'>WebKitGTK</a>"
        "    <div role='link'>Link</div>"
        "    <p>This is <button>button1</button> and <button>button2</button> in a paragraph</p>"
        "    <p>This is <a href='https://www.webkitgtk.org'>link1</a> and <a href='https://www.gnome.org'>link2</a> in paragraph</p>"
        "    <ul><li>List item</li></ul>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 5);

    auto section = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(section.get()));
    g_assert_cmpint(atspi_accessible_get_role(section.get(), nullptr), ==, ATSPI_ROLE_SECTION);
    g_assert_cmpint(atspi_accessible_get_child_count(section.get(), nullptr), ==, 1);

    auto a = adoptGRef(atspi_accessible_get_child_at_index(section.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(a.get()));
    g_assert_cmpint(atspi_accessible_get_role(a.get(), nullptr), ==, ATSPI_ROLE_LINK);
    auto link = adoptGRef(atspi_accessible_get_hyperlink(a.get()));
    g_assert_true(ATSPI_IS_HYPERLINK(link.get()));
    g_assert_cmpint(atspi_hyperlink_get_n_anchors(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    g_assert_true(atspi_hyperlink_is_valid(ATSPI_HYPERLINK(link.get()), nullptr));
    g_assert_cmpint(atspi_hyperlink_get_start_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 0);
    g_assert_cmpint(atspi_hyperlink_get_end_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    GUniquePtr<char> uri(atspi_hyperlink_get_uri(ATSPI_HYPERLINK(link.get()), 0, nullptr));
    g_assert_cmpstr(uri.get(), ==, "https://www.webkitgtk.org/");
    g_assert_true(atspi_hyperlink_get_object(ATSPI_HYPERLINK(link.get()), 0, nullptr) == a.get());

    auto div = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(div.get()));
    g_assert_cmpint(atspi_accessible_get_role(div.get(), nullptr), ==, ATSPI_ROLE_LINK);
    link = adoptGRef(atspi_accessible_get_hyperlink(div.get()));
    g_assert_true(ATSPI_IS_HYPERLINK(link.get()));
    g_assert_cmpint(atspi_hyperlink_get_n_anchors(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    g_assert_true(atspi_hyperlink_is_valid(ATSPI_HYPERLINK(link.get()), nullptr));
    g_assert_cmpint(atspi_hyperlink_get_start_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 0);
    g_assert_cmpint(atspi_hyperlink_get_end_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    uri.reset(atspi_hyperlink_get_uri(ATSPI_HYPERLINK(link.get()), 0, nullptr));
    g_assert_cmpstr(uri.get(), ==, "");
    g_assert_true(atspi_hyperlink_get_object(ATSPI_HYPERLINK(link.get()), 0, nullptr) == div.get());

    auto p = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 2, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(p.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(p.get(), nullptr), ==, 2);
    auto button1 = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(button1.get()));
    link = adoptGRef(atspi_accessible_get_hyperlink(button1.get()));
    g_assert_true(ATSPI_IS_HYPERLINK(link.get()));
    g_assert_cmpint(atspi_hyperlink_get_n_anchors(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    g_assert_true(atspi_hyperlink_is_valid(ATSPI_HYPERLINK(link.get()), nullptr));
    g_assert_cmpint(atspi_hyperlink_get_start_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 8);
    g_assert_cmpint(atspi_hyperlink_get_end_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 9);
    uri.reset(atspi_hyperlink_get_uri(ATSPI_HYPERLINK(link.get()), 0, nullptr));
    g_assert_cmpstr(uri.get(), ==, "");
    g_assert_true(atspi_hyperlink_get_object(ATSPI_HYPERLINK(link.get()), 0, nullptr) == button1.get());
    auto button2 = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(button2.get()));
    link = adoptGRef(atspi_accessible_get_hyperlink(button2.get()));
    g_assert_true(ATSPI_IS_HYPERLINK(link.get()));
    g_assert_cmpint(atspi_hyperlink_get_n_anchors(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    g_assert_true(atspi_hyperlink_is_valid(ATSPI_HYPERLINK(link.get()), nullptr));
    g_assert_cmpint(atspi_hyperlink_get_start_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 14);
    g_assert_cmpint(atspi_hyperlink_get_end_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 15);
    uri.reset(atspi_hyperlink_get_uri(ATSPI_HYPERLINK(link.get()), 0, nullptr));
    g_assert_cmpstr(uri.get(), ==, "");
    g_assert_true(atspi_hyperlink_get_object(ATSPI_HYPERLINK(link.get()), 0, nullptr) == button2.get());

    p = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 3, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(p.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(p.get(), nullptr), ==, 2);
    auto link1 = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(link1.get()));
    link = adoptGRef(atspi_accessible_get_hyperlink(link1.get()));
    g_assert_true(ATSPI_IS_HYPERLINK(link.get()));
    g_assert_cmpint(atspi_hyperlink_get_n_anchors(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    g_assert_true(atspi_hyperlink_is_valid(ATSPI_HYPERLINK(link.get()), nullptr));
    g_assert_cmpint(atspi_hyperlink_get_start_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 8);
    g_assert_cmpint(atspi_hyperlink_get_end_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 9);
    uri.reset(atspi_hyperlink_get_uri(ATSPI_HYPERLINK(link.get()), 0, nullptr));
    g_assert_cmpstr(uri.get(), ==, "https://www.webkitgtk.org/");
    g_assert_true(atspi_hyperlink_get_object(ATSPI_HYPERLINK(link.get()), 0, nullptr) == link1.get());
    auto link2 = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(link2.get()));
    link = adoptGRef(atspi_accessible_get_hyperlink(link2.get()));
    g_assert_true(ATSPI_IS_HYPERLINK(link.get()));
    g_assert_cmpint(atspi_hyperlink_get_n_anchors(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    g_assert_true(atspi_hyperlink_is_valid(ATSPI_HYPERLINK(link.get()), nullptr));
    g_assert_cmpint(atspi_hyperlink_get_start_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 14);
    g_assert_cmpint(atspi_hyperlink_get_end_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 15);
    uri.reset(atspi_hyperlink_get_uri(ATSPI_HYPERLINK(link.get()), 0, nullptr));
    g_assert_cmpstr(uri.get(), ==, "https://www.gnome.org/");
    g_assert_true(atspi_hyperlink_get_object(ATSPI_HYPERLINK(link.get()), 0, nullptr) == link2.get());

    auto ul = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 4, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(ul.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(ul.get(), nullptr), ==, 1);
    auto li = adoptGRef(atspi_accessible_get_child_at_index(ul.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(li.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(li.get(), nullptr), ==, 1);
    auto marker = adoptGRef(atspi_accessible_get_child_at_index(li.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(marker.get()));
    link = adoptGRef(atspi_accessible_get_hyperlink(marker.get()));
    g_assert_true(ATSPI_IS_HYPERLINK(link.get()));
    g_assert_cmpint(atspi_hyperlink_get_n_anchors(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    g_assert_true(atspi_hyperlink_is_valid(ATSPI_HYPERLINK(link.get()), nullptr));
    g_assert_cmpint(atspi_hyperlink_get_start_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 0);
    g_assert_cmpint(atspi_hyperlink_get_end_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    uri.reset(atspi_hyperlink_get_uri(ATSPI_HYPERLINK(link.get()), 0, nullptr));
    g_assert_cmpstr(uri.get(), ==, "");
    g_assert_true(atspi_hyperlink_get_object(ATSPI_HYPERLINK(link.get()), 0, nullptr) == marker.get());
}

static void testHypertextBasic(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>This is <button>button</button> and <a href='https://www.webkitgtk.org'>link</a> in a paragraph</p>"
        "    <ol><li>List with a <a href='https://www.webkit.org'>link</a></li></ol>"
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
    g_assert_true(ATSPI_IS_HYPERTEXT(p.get()));
    g_assert_cmpint(atspi_hypertext_get_n_links(ATSPI_HYPERTEXT(p.get()), nullptr), ==, 2);

    auto link = adoptGRef(atspi_hypertext_get_link(ATSPI_HYPERTEXT(p.get()), 0, nullptr));
    g_assert_true(ATSPI_IS_HYPERLINK(link.get()));
    g_assert_cmpint(atspi_hyperlink_get_n_anchors(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    g_assert_true(atspi_hyperlink_is_valid(ATSPI_HYPERLINK(link.get()), nullptr));
    g_assert_cmpint(atspi_hyperlink_get_start_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 8);
    g_assert_cmpint(atspi_hyperlink_get_end_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 9);
    GUniquePtr<char> uri(atspi_hyperlink_get_uri(ATSPI_HYPERLINK(link.get()), 0, nullptr));
    g_assert_cmpstr(uri.get(), ==, "");
    auto button = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 0, nullptr));
    g_assert_true(atspi_hyperlink_get_object(ATSPI_HYPERLINK(link.get()), 0, nullptr) == button.get());

    link = adoptGRef(atspi_hypertext_get_link(ATSPI_HYPERTEXT(p.get()), 1, nullptr));
    g_assert_true(ATSPI_IS_HYPERLINK(link.get()));
    g_assert_cmpint(atspi_hyperlink_get_n_anchors(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    g_assert_true(atspi_hyperlink_is_valid(ATSPI_HYPERLINK(link.get()), nullptr));
    g_assert_cmpint(atspi_hyperlink_get_start_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 14);
    g_assert_cmpint(atspi_hyperlink_get_end_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 15);
    uri.reset(atspi_hyperlink_get_uri(ATSPI_HYPERLINK(link.get()), 0, nullptr));
    g_assert_cmpstr(uri.get(), ==, "https://www.webkitgtk.org/");
    auto a = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 1, nullptr));
    g_assert_true(atspi_hyperlink_get_object(ATSPI_HYPERLINK(link.get()), 0, nullptr) == a.get());

    g_assert_cmpint(atspi_hypertext_get_link_index(ATSPI_HYPERTEXT(p.get()), 0, nullptr), ==, -1);
    g_assert_cmpint(atspi_hypertext_get_link_index(ATSPI_HYPERTEXT(p.get()), 8, nullptr), ==, 0);
    g_assert_cmpint(atspi_hypertext_get_link_index(ATSPI_HYPERTEXT(p.get()), 9, nullptr), ==, -1);
    g_assert_cmpint(atspi_hypertext_get_link_index(ATSPI_HYPERTEXT(p.get()), 14, nullptr), ==, 1);
    g_assert_cmpint(atspi_hypertext_get_link_index(ATSPI_HYPERTEXT(p.get()), 15, nullptr), ==, -1);

    auto ol = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(ol.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(ol.get(), nullptr), ==, 1);
    auto li = adoptGRef(atspi_accessible_get_child_at_index(ol.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_HYPERTEXT(li.get()));
    g_assert_cmpint(atspi_hypertext_get_n_links(ATSPI_HYPERTEXT(li.get()), nullptr), ==, 2);
    link = adoptGRef(atspi_hypertext_get_link(ATSPI_HYPERTEXT(li.get()), 0, nullptr));
    g_assert_true(ATSPI_IS_HYPERLINK(link.get()));
    g_assert_cmpint(atspi_hyperlink_get_n_anchors(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    g_assert_true(atspi_hyperlink_is_valid(ATSPI_HYPERLINK(link.get()), nullptr));
    g_assert_cmpint(atspi_hyperlink_get_start_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 0);
    g_assert_cmpint(atspi_hyperlink_get_end_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    uri.reset(atspi_hyperlink_get_uri(ATSPI_HYPERLINK(link.get()), 0, nullptr));
    g_assert_cmpstr(uri.get(), ==, "");
    auto marker = adoptGRef(atspi_accessible_get_child_at_index(li.get(), 0, nullptr));
    g_assert_true(atspi_hyperlink_get_object(ATSPI_HYPERLINK(link.get()), 0, nullptr) == marker.get());
    link = adoptGRef(atspi_hypertext_get_link(ATSPI_HYPERTEXT(li.get()), 1, nullptr));
    g_assert_true(ATSPI_IS_HYPERLINK(link.get()));
    g_assert_cmpint(atspi_hyperlink_get_n_anchors(ATSPI_HYPERLINK(link.get()), nullptr), ==, 1);
    g_assert_true(atspi_hyperlink_is_valid(ATSPI_HYPERLINK(link.get()), nullptr));
    g_assert_cmpint(atspi_hyperlink_get_start_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 13);
    g_assert_cmpint(atspi_hyperlink_get_end_index(ATSPI_HYPERLINK(link.get()), nullptr), ==, 14);
    uri.reset(atspi_hyperlink_get_uri(ATSPI_HYPERLINK(link.get()), 0, nullptr));
    g_assert_cmpstr(uri.get(), ==, "https://www.webkit.org/");
    a = adoptGRef(atspi_accessible_get_child_at_index(li.get(), 1, nullptr));
    g_assert_true(atspi_hyperlink_get_object(ATSPI_HYPERLINK(link.get()), 0, nullptr) == a.get());

    g_assert_cmpint(atspi_hypertext_get_link_index(ATSPI_HYPERTEXT(li.get()), 0, nullptr), ==, 0);
    g_assert_cmpint(atspi_hypertext_get_link_index(ATSPI_HYPERTEXT(li.get()), 1, nullptr), ==, -1);
    g_assert_cmpint(atspi_hypertext_get_link_index(ATSPI_HYPERTEXT(li.get()), 13, nullptr), ==, 1);
    g_assert_cmpint(atspi_hypertext_get_link_index(ATSPI_HYPERTEXT(li.get()), 14, nullptr), ==, -1);
}

static void testActionBasic(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>This is <button accessKey='p'>button</button> and <a href='https://www.webkitgtk.org'>link</a> in a paragraph</p>"
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
    g_assert_true(ATSPI_IS_ACTION(p.get()));
    // Paragraph implements action interface, but it does nothing.
    g_assert_cmpint(atspi_action_get_n_actions(ATSPI_ACTION(p.get()), nullptr), ==, 1);
    GUniquePtr<char> name(atspi_action_get_action_name(ATSPI_ACTION(p.get()), 0, nullptr));
    g_assert_cmpstr(name.get(), ==, "");
    GUniquePtr<char> localizedName(atspi_action_get_localized_name(ATSPI_ACTION(p.get()), 0, nullptr));
    g_assert_cmpstr(localizedName.get(), ==, "");
    GUniquePtr<char> keyBinding(atspi_action_get_key_binding(ATSPI_ACTION(p.get()), 0, nullptr));
    g_assert_cmpstr(keyBinding.get(), ==, "");

    auto button = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACTION(button.get()));
    g_assert_cmpint(atspi_action_get_n_actions(ATSPI_ACTION(button.get()), nullptr), ==, 1);
    name.reset(atspi_action_get_action_name(ATSPI_ACTION(button.get()), 0, nullptr));
    g_assert_cmpstr(name.get(), ==, "press");
    localizedName.reset(atspi_action_get_localized_name(ATSPI_ACTION(button.get()), 0, nullptr));
    g_assert_cmpstr(localizedName.get(), ==, "press");
    keyBinding.reset(atspi_action_get_key_binding(ATSPI_ACTION(button.get()), 0, nullptr));
    g_assert_cmpstr(keyBinding.get(), ==, "p");

    auto a = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACTION(a.get()));
    g_assert_cmpint(atspi_action_get_n_actions(ATSPI_ACTION(a.get()), nullptr), ==, 1);
    name.reset(atspi_action_get_action_name(ATSPI_ACTION(a.get()), 0, nullptr));
    g_assert_cmpstr(name.get(), ==, "jump");
    localizedName.reset(atspi_action_get_localized_name(ATSPI_ACTION(a.get()), 0, nullptr));
    g_assert_cmpstr(localizedName.get(), ==, "jump");
    keyBinding.reset(atspi_action_get_key_binding(ATSPI_ACTION(a.get()), 0, nullptr));
    g_assert_cmpstr(keyBinding.get(), ==, "");
}

static void testDocumentBasic(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<!doctype html>"
        "<html>"
        "  <head>"
        "    <meta http-equiv='content-language' content='en-us'/>"
        "    <meta http-equiv='content-type' content='text/html; charset=UTF-8'/>"
        "    <title>Document attributes</title>"
        "  </head>"
        "  <body>"
        "    <p>This is a paragraph</p>"
        "    <p lang='es'>Spanish</p>"
        "  </body>"
        "</html>",
        "http://example.org");
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_DOCUMENT(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 2);

    GUniquePtr<char> language(atspi_document_get_locale(ATSPI_DOCUMENT(documentWeb.get()), nullptr));
    g_assert_cmpstr(language.get(), ==, "en-us");
    // Elements with no language attribute inhewir the document one.
    auto p1 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(p1.get()));
    g_assert_cmpstr(atspi_accessible_get_object_locale(p1.get(), nullptr), ==, "en-us");
    auto p2 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(p2.get()));
    g_assert_cmpstr(atspi_accessible_get_object_locale(p2.get(), nullptr), ==, "es");

    GRefPtr<GHashTable> attributes = adoptGRef(atspi_document_get_attributes(ATSPI_DOCUMENT(documentWeb.get()), nullptr));
    g_assert_nonnull(attributes.get());
    g_assert_cmpuint(g_hash_table_size(attributes.get()), ==, 5);
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "DocType")), ==, "html");
    GUniquePtr<char> value(atspi_document_get_document_attribute_value(ATSPI_DOCUMENT(documentWeb.get()), const_cast<char*>("DocType"), nullptr));
    g_assert_cmpstr(value.get(), ==, "html");
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "Encoding")), ==, "UTF-8");
    value.reset(atspi_document_get_document_attribute_value(ATSPI_DOCUMENT(documentWeb.get()), const_cast<char*>("Encoding"), nullptr));
    g_assert_cmpstr(value.get(), ==, "UTF-8");
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "URI")), ==, "http://example.org/");
    value.reset(atspi_document_get_document_attribute_value(ATSPI_DOCUMENT(documentWeb.get()), const_cast<char*>("URI"), nullptr));
    g_assert_cmpstr(value.get(), ==, "http://example.org/");
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "MimeType")), ==, "text/html");
    value.reset(atspi_document_get_document_attribute_value(ATSPI_DOCUMENT(documentWeb.get()), const_cast<char*>("MimeType"), nullptr));
    g_assert_cmpstr(value.get(), ==, "text/html");
    g_assert_cmpstr(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "Title")), ==, "Document attributes");
    value.reset(atspi_document_get_document_attribute_value(ATSPI_DOCUMENT(documentWeb.get()), const_cast<char*>("Title"), nullptr));
    g_assert_cmpstr(value.get(), ==, "Document attributes");
}

static void testDocumentLoadEvents(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow();
    test->loadURI("about:blank");
    test->waitUntilLoadFinished();
    test->startEventMonitor(std::nullopt, { "document:", "object:state-changed:busy" });
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <p>Loading events test</p>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();
    auto events = test->stopEventMonitor(2);
    g_assert_cmpuint(events.size(), ==, 2);
    g_assert_cmpstr(events[0]->type, ==, "object:state-changed:busy");
    g_assert_cmpuint(events[0]->detail1, ==, 0);
    g_assert_true(ATSPI_IS_ACCESSIBLE(events[0]->source));
    g_assert_cmpint(atspi_accessible_get_role(events[0]->source, nullptr), ==, ATSPI_ROLE_DOCUMENT_WEB);
    g_assert_cmpstr(events[1]->type, ==, "document:load-complete");
    g_assert_true(events[0]->source == events[1]->source);
    g_assert_true(ATSPI_IS_ACCESSIBLE(events[1]->source));
    g_assert_cmpint(atspi_accessible_get_role(events[1]->source, nullptr), ==, ATSPI_ROLE_DOCUMENT_WEB);
    events = { };

    test->startEventMonitor(std::nullopt, { "document:", "object:state-changed:busy" });
    webkit_web_view_reload(test->m_webView);
    test->waitUntilLoadFinished();
    events = test->stopEventMonitor(4);
    g_assert_cmpuint(events.size(), ==, 4);
    g_assert_cmpstr(events[0]->type, ==, "object:state-changed:busy");
    g_assert_cmpuint(events[0]->detail1, ==, 1);
    g_assert_cmpstr(events[1]->type, ==, "document:reload");
    g_assert_true(events[0]->source == events[1]->source);
    g_assert_true(ATSPI_IS_ACCESSIBLE(events[0]->source));
    g_assert_cmpint(atspi_accessible_get_role(events[0]->source, nullptr), ==, ATSPI_ROLE_DOCUMENT_WEB);
    g_assert_cmpstr(events[2]->type, ==, "object:state-changed:busy");
    g_assert_cmpuint(events[2]->detail1, ==, 0);
    g_assert_false(events[1]->source == events[2]->source);
    g_assert_cmpstr(events[3]->type, ==, "document:load-complete");
    g_assert_true(events[2]->source == events[3]->source);
    g_assert_true(ATSPI_IS_ACCESSIBLE(events[2]->source));
    g_assert_cmpint(atspi_accessible_get_role(events[2]->source, nullptr), ==, ATSPI_ROLE_DOCUMENT_WEB);
    events = { };
}

static void testImageBasic(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    GUniquePtr<char> baseDir(g_strdup_printf("file://%s/", Test::getResourcesDir().data()));
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <img style='position:absolute; left:1; top:1' src='blank.ico' width=5 height=5 alt='This is a blank icon' lang='en'></img>"
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
    g_assert_true(ATSPI_IS_IMAGE(img.get()));
    GUniquePtr<AtspiRect> rect(atspi_image_get_image_extents(ATSPI_IMAGE(img.get()), ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_nonnull(rect.get());
    g_assert_cmpuint(rect->x, ==, 1);
    g_assert_cmpuint(rect->y, ==, 1);
    g_assert_cmpuint(rect->width, ==, 5);
    g_assert_cmpuint(rect->height, ==, 5);
    GUniquePtr<AtspiPoint> point(atspi_image_get_image_position(ATSPI_IMAGE(img.get()), ATSPI_COORD_TYPE_WINDOW, nullptr));
    g_assert_nonnull(point.get());
    g_assert_cmpuint(rect->x, ==, point->x);
    g_assert_cmpuint(rect->y, ==, point->y);
    GUniquePtr<AtspiPoint> size(atspi_image_get_image_size(ATSPI_IMAGE(img.get()), nullptr));
    g_assert_nonnull(size.get());
    g_assert_cmpuint(size->x, ==, rect->width);
    g_assert_cmpuint(size->y, ==, rect->height);

    GUniquePtr<char> description(atspi_image_get_image_description(ATSPI_IMAGE(img.get()), nullptr));
    g_assert_cmpstr(description.get(), ==, "This is a blank icon");
    GUniquePtr<char> locale(atspi_image_get_image_locale(ATSPI_IMAGE(img.get()), nullptr));
    g_assert_cmpstr(locale.get(), ==, "en");
}

static void testSelectionListBox(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <select id='single' size='3'>"
        "      <option>Option 1</option>"
        "      <option disabled>Option 2</option>"
        "      <option>Option 3</option>"
        "    </select>"
        "    <select id='multiple' size='3' multiple>"
        "      <option>Option 1</option>"
        "      <option selected>Option 2</option>"
        "      <option>Option 3</option>"
        "    </select>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    auto panel = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(panel.get()));
    g_assert_cmpint(atspi_accessible_get_role(panel.get(), nullptr), ==, ATSPI_ROLE_PANEL);
    g_assert_cmpint(atspi_accessible_get_child_count(panel.get(), nullptr), ==, 2);

    test->runJavaScriptAndWaitUntilFinished("document.getElementById('single').focus();", nullptr);

    auto listBox = adoptGRef(atspi_accessible_get_child_at_index(panel.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_SELECTION(listBox.get()));
    g_assert_cmpint(atspi_accessible_get_role(listBox.get(), nullptr), ==, ATSPI_ROLE_LIST_BOX);
    g_assert_cmpint(atspi_accessible_get_child_count(listBox.get(), nullptr), ==, 3);
    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(listBox.get()), nullptr), ==, 0);
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 1, nullptr));
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 2, nullptr));
    g_assert_false(atspi_selection_select_child(ATSPI_SELECTION(listBox.get()), 1, nullptr));
    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(listBox.get()), nullptr), ==, 0);
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 1, nullptr));
    test->startEventMonitor(listBox.get(), { "object:selection-changed" });
    g_assert_true(atspi_selection_select_child(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    auto events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:selection-changed");
    events = { };
    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(listBox.get()), nullptr), ==, 1);
    g_assert_true(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    auto selectedChild = adoptGRef(atspi_selection_get_selected_child(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(selectedChild.get()));
    auto option1 = adoptGRef(atspi_accessible_get_child_at_index(listBox.get(), 0, nullptr));
    g_assert_true(selectedChild.get() == option1.get());
    g_assert_true(AccessibilityTest::isSelected(option1.get()));
    test->startEventMonitor(listBox.get(), { "object:selection-changed" });
    g_assert_true(atspi_selection_select_child(ATSPI_SELECTION(listBox.get()), 2, nullptr));
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:selection-changed");
    events = { };
    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(listBox.get()), nullptr), ==, 1);
    g_assert_true(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 2, nullptr));
    selectedChild = adoptGRef(atspi_selection_get_selected_child(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(selectedChild.get()));
    auto option3 = adoptGRef(atspi_accessible_get_child_at_index(listBox.get(), 2, nullptr));
    g_assert_true(selectedChild.get() == option3.get());
    g_assert_true(AccessibilityTest::isSelected(option3.get()));
    g_assert_false(AccessibilityTest::isSelected(option1.get()));
    g_assert_false(atspi_selection_deselect_selected_child(ATSPI_SELECTION(listBox.get()), 1, nullptr));
    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(listBox.get()), nullptr), ==, 1);
    g_assert_true(atspi_selection_deselect_selected_child(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(listBox.get()), nullptr), ==, 0);
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 2, nullptr));
    g_assert_false(atspi_selection_select_all(ATSPI_SELECTION(listBox.get()), nullptr));
    g_assert_false(AccessibilityTest::isSelected(option1.get()));
    g_assert_false(AccessibilityTest::isSelected(option3.get()));
    g_assert_true(atspi_selection_select_child(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(listBox.get()), nullptr), ==, 1);
    g_assert_true(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    test->startEventMonitor(listBox.get(), { "object:selection-changed" });
    g_assert_true(atspi_selection_clear_selection(ATSPI_SELECTION(listBox.get()), nullptr));
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:selection-changed");
    events = { };
    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(listBox.get()), nullptr), ==, 0);
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 0, nullptr));

    test->runJavaScriptAndWaitUntilFinished("document.getElementById('multiple').focus();", nullptr);

    listBox = adoptGRef(atspi_accessible_get_child_at_index(panel.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_SELECTION(listBox.get()));
    g_assert_cmpint(atspi_accessible_get_role(listBox.get(), nullptr), ==, ATSPI_ROLE_LIST_BOX);
    g_assert_cmpint(atspi_accessible_get_child_count(listBox.get(), nullptr), ==, 3);
    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(listBox.get()), nullptr), ==, 1);
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    g_assert_true(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 1, nullptr));
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 2, nullptr));
    selectedChild = adoptGRef(atspi_selection_get_selected_child(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(selectedChild.get()));
    auto option2 = adoptGRef(atspi_accessible_get_child_at_index(listBox.get(), 1, nullptr));
    g_assert_true(selectedChild.get() == option2.get());
    g_assert_true(AccessibilityTest::isSelected(option2.get()));
    g_assert_true(atspi_selection_select_child(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(listBox.get()), nullptr), ==, 2);
    g_assert_true(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    g_assert_true(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 1, nullptr));
    selectedChild = adoptGRef(atspi_selection_get_selected_child(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    option1 = adoptGRef(atspi_accessible_get_child_at_index(listBox.get(), 0, nullptr));
    g_assert_true(selectedChild.get() == option1.get());
    g_assert_true(AccessibilityTest::isSelected(option1.get()));
    g_assert_true(AccessibilityTest::isSelected(option2.get()));
    selectedChild = adoptGRef(atspi_selection_get_selected_child(ATSPI_SELECTION(listBox.get()), 1, nullptr));
    g_assert_true(selectedChild.get() == option2.get());
    g_assert_true(atspi_selection_deselect_child(ATSPI_SELECTION(listBox.get()), 1, nullptr));
    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(listBox.get()), nullptr), ==, 1);
    g_assert_true(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 1, nullptr));
    test->startEventMonitor(listBox.get(), { "object:selection-changed" });
    g_assert_true(atspi_selection_select_all(ATSPI_SELECTION(listBox.get()), nullptr));
    events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:selection-changed");
    events = { };
    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(listBox.get()), nullptr), ==, 3);
    g_assert_true(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    g_assert_true(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 1, nullptr));
    g_assert_true(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 2, nullptr));
    g_assert_true(atspi_selection_clear_selection(ATSPI_SELECTION(listBox.get()), nullptr));
    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(listBox.get()), nullptr), ==, 0);
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 0, nullptr));
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 1, nullptr));
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(listBox.get()), 2, nullptr));
}

static void testSelectionMenuList(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <select id='combo'>"
        "      <option>Option 1</option>"
        "      <option selected>Option 2</option>"
        "      <option>Option 3</option>"
        "      <option disabled>Option 4</option>"
        "      <option>Option 5</option>"
        "    </select>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    auto panel = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(panel.get()));
    g_assert_cmpint(atspi_accessible_get_role(panel.get(), nullptr), ==, ATSPI_ROLE_PANEL);
    g_assert_cmpint(atspi_accessible_get_child_count(panel.get(), nullptr), ==, 1);

    test->runJavaScriptAndWaitUntilFinished("document.getElementById('combo').focus();", nullptr);

    auto combo = adoptGRef(atspi_accessible_get_child_at_index(panel.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(combo.get()));
    g_assert_cmpint(atspi_accessible_get_role(combo.get(), nullptr), ==, ATSPI_ROLE_COMBO_BOX);
    g_assert_cmpint(atspi_accessible_get_child_count(combo.get(), nullptr), ==, 1);

    auto menuList = adoptGRef(atspi_accessible_get_child_at_index(combo.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_SELECTION(menuList.get()));
    g_assert_cmpint(atspi_accessible_get_role(menuList.get(), nullptr), ==, ATSPI_ROLE_MENU);
    g_assert_cmpint(atspi_accessible_get_child_count(menuList.get(), nullptr), ==, 5);

    g_assert_cmpint(atspi_selection_get_n_selected_children(ATSPI_SELECTION(menuList.get()), nullptr), ==, 1);
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(menuList.get()), 0, nullptr));
    g_assert_true(atspi_selection_is_child_selected(ATSPI_SELECTION(menuList.get()), 1, nullptr));
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(menuList.get()), 2, nullptr));
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(menuList.get()), 3, nullptr));
    g_assert_false(atspi_selection_is_child_selected(ATSPI_SELECTION(menuList.get()), 4, nullptr));
    auto selectedChild = adoptGRef(atspi_selection_get_selected_child(ATSPI_SELECTION(menuList.get()), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(selectedChild.get()));
    auto option2 = adoptGRef(atspi_accessible_get_child_at_index(menuList.get(), 1, nullptr));
    g_assert_true(selectedChild.get() == option2.get());
    g_assert_true(AccessibilityTest::isSelected(option2.get()));
    auto option1 = adoptGRef(atspi_accessible_get_child_at_index(menuList.get(), 0, nullptr));
    g_assert_false(AccessibilityTest::isSelected(option1.get()));
    g_assert_false(atspi_selection_select_child(ATSPI_SELECTION(menuList.get()), 3, nullptr));
    test->startEventMonitor(menuList.get(), { "object:selection-changed" });
    g_assert_true(atspi_selection_select_child(ATSPI_SELECTION(menuList.get()), 0, nullptr));
    auto events = test->stopEventMonitor(1);
    g_assert_cmpuint(events.size(), ==, 1);
    g_assert_cmpstr(events[0]->type, ==, "object:selection-changed");
    events = { };
    g_assert_true(atspi_selection_is_child_selected(ATSPI_SELECTION(menuList.get()), 0, nullptr));
    selectedChild = adoptGRef(atspi_selection_get_selected_child(ATSPI_SELECTION(menuList.get()), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(selectedChild.get()));
    g_assert_true(selectedChild.get() == option1.get());
    g_assert_true(AccessibilityTest::isSelected(option1.get()));
    g_assert_false(AccessibilityTest::isSelected(option2.get()));
    g_assert_false(atspi_selection_deselect_selected_child(ATSPI_SELECTION(menuList.get()), 0, nullptr));
    g_assert_false(atspi_selection_deselect_child(ATSPI_SELECTION(menuList.get()), 0, nullptr));
    g_assert_false(atspi_selection_select_all(ATSPI_SELECTION(menuList.get()), nullptr));
    g_assert_false(atspi_selection_clear_selection(ATSPI_SELECTION(menuList.get()), nullptr));
}

static void testTableBasic(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <table>"
        "      <caption>Table Caption</caption>"
        "      <tr><th>Column 1</th><th>Column 2</th><th>Column 3</th></tr>"
        "      <tr><th rowspan='2'>Row 1 Cell 1</th><td>Row 1 Cell 2</td><td>Row 1 Cell 3</td></tr>"
        "      <tr><td>Row 2 Cell 2</td><td>Row 2 Cell 3</td></tr>"
        "      <tr><td colspan='3'>Row 3 Cell 1</td></tr>"
        "    </table>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 1);

    auto table = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_TABLE(table.get()));
    g_assert_cmpint(atspi_accessible_get_role(table.get(), nullptr), ==, ATSPI_ROLE_TABLE);
    g_assert_cmpint(atspi_accessible_get_child_count(table.get(), nullptr), ==, 5);
    g_assert_cmpint(atspi_table_get_n_rows(ATSPI_TABLE(table.get()), nullptr), ==, 4);
    g_assert_cmpint(atspi_table_get_n_columns(ATSPI_TABLE(table.get()), nullptr), ==, 3);

    auto caption = adoptGRef(atspi_table_get_caption(ATSPI_TABLE(table.get()), nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(caption.get()));
    g_assert_cmpint(atspi_accessible_get_role(caption.get(), nullptr), ==, ATSPI_ROLE_CAPTION);
    g_assert_true(ATSPI_IS_TEXT(caption.get()));
    GUniquePtr<char> text(atspi_text_get_text(ATSPI_TEXT(caption.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "Table Caption");

    text.reset(atspi_table_get_row_description(ATSPI_TABLE(table.get()), 0, nullptr));
    g_assert_cmpstr(text.get(), ==, "");
    g_assert_null(atspi_table_get_row_header(ATSPI_TABLE(table.get()), 0, nullptr));
    text.reset(atspi_table_get_row_description(ATSPI_TABLE(table.get()), 1, nullptr));
    g_assert_cmpstr(text.get(), ==, "Row 1 Cell 1");
    auto rowHeader = adoptGRef(atspi_table_get_row_header(ATSPI_TABLE(table.get()), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(rowHeader.get()));
    text.reset(atspi_table_get_row_description(ATSPI_TABLE(table.get()), 2, nullptr));
    g_assert_cmpstr(text.get(), ==, "Row 1 Cell 1");
    auto header = adoptGRef(atspi_table_get_row_header(ATSPI_TABLE(table.get()), 2, nullptr));
    g_assert_true(rowHeader.get() == header.get());
    text.reset(atspi_table_get_row_description(ATSPI_TABLE(table.get()), 3, nullptr));
    g_assert_cmpstr(text.get(), ==, "");
    g_assert_null(atspi_table_get_row_header(ATSPI_TABLE(table.get()), 3, nullptr));

    text.reset(atspi_table_get_column_description(ATSPI_TABLE(table.get()), 0, nullptr));
    g_assert_cmpstr(text.get(), ==, "Column 1");
    auto columnHeader0 = adoptGRef(atspi_table_get_column_header(ATSPI_TABLE(table.get()), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(columnHeader0.get()));
    text.reset(atspi_table_get_column_description(ATSPI_TABLE(table.get()), 1, nullptr));
    g_assert_cmpstr(text.get(), ==, "Column 2");
    auto columnHeader1 = adoptGRef(atspi_table_get_column_header(ATSPI_TABLE(table.get()), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(columnHeader1.get()));
    text.reset(atspi_table_get_column_description(ATSPI_TABLE(table.get()), 2, nullptr));
    g_assert_cmpstr(text.get(), ==, "Column 3");
    auto columnHeader2 = adoptGRef(atspi_table_get_column_header(ATSPI_TABLE(table.get()), 2, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(columnHeader2.get()));

    g_assert_cmpint(atspi_table_get_index_at(ATSPI_TABLE(table.get()), 0, 0, nullptr), ==, 0);
    g_assert_cmpint(atspi_table_get_row_at_index(ATSPI_TABLE(table.get()), 0, nullptr), ==, 0);
    g_assert_cmpint(atspi_table_get_column_at_index(ATSPI_TABLE(table.get()), 0, nullptr), ==, 0);
    g_assert_cmpint(atspi_table_get_row_extent_at(ATSPI_TABLE(table.get()), 0, 0, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_column_extent_at(ATSPI_TABLE(table.get()), 0, 0, nullptr), ==, 1);
    int row, column, rowSpan, columnSpan;
    gboolean isSelected;
    g_assert_true(atspi_table_get_row_column_extents_at_index(ATSPI_TABLE(table.get()), 0, &row, &column, &rowSpan, &columnSpan, &isSelected, nullptr));
    g_assert_cmpint(row, ==, 0);
    g_assert_cmpint(column, ==, 0);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    g_assert_false(isSelected);
    auto cell0 = adoptGRef(atspi_table_get_accessible_at(ATSPI_TABLE(table.get()), 0, 0, nullptr));
    g_assert_true(ATSPI_IS_TABLE_CELL(cell0.get()));
    g_assert_true(cell0.get() == columnHeader0.get());
    g_assert_cmpint(atspi_table_cell_get_row_span(ATSPI_TABLE_CELL(cell0.get()), nullptr), ==, 1);
    g_assert_cmpint(atspi_table_cell_get_column_span(ATSPI_TABLE_CELL(cell0.get()), nullptr), ==, 1);
    g_assert_true(atspi_table_cell_get_position(ATSPI_TABLE_CELL(cell0.get()), &row, &column, nullptr));
    g_assert_cmpint(row, ==, 0);
    g_assert_cmpint(column, ==, 0);
    atspi_table_cell_get_row_column_span(ATSPI_TABLE_CELL(cell0.get()), &row, &column, &rowSpan, &columnSpan, nullptr);
    g_assert_cmpint(row, ==, 0);
    g_assert_cmpint(column, ==, 0);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    auto cellTable = adoptGRef(atspi_table_cell_get_table(ATSPI_TABLE_CELL(cell0.get()), nullptr));
    g_assert_true(table.get() == cellTable.get());
    GRefPtr<GPtrArray> rowHeaders = adoptGRef(atspi_table_cell_get_row_header_cells(ATSPI_TABLE_CELL(cell0.get()), nullptr));
    g_assert_cmpint(rowHeaders->len, ==, 0);
    GRefPtr<GPtrArray> columnHeaders = adoptGRef(atspi_table_cell_get_column_header_cells(ATSPI_TABLE_CELL(cell0.get()), nullptr));
    g_assert_cmpint(columnHeaders->len, ==, 0);
    g_assert_true(ATSPI_IS_TEXT(cell0.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(cell0.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "Column 1");

    g_assert_cmpint(atspi_table_get_index_at(ATSPI_TABLE(table.get()), 0, 1, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_row_at_index(ATSPI_TABLE(table.get()), 1, nullptr), ==, 0);
    g_assert_cmpint(atspi_table_get_column_at_index(ATSPI_TABLE(table.get()), 1, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_row_extent_at(ATSPI_TABLE(table.get()), 0, 1, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_column_extent_at(ATSPI_TABLE(table.get()), 0, 1, nullptr), ==, 1);
    g_assert_true(atspi_table_get_row_column_extents_at_index(ATSPI_TABLE(table.get()), 1, &row, &column, &rowSpan, &columnSpan, &isSelected, nullptr));
    g_assert_cmpint(row, ==, 0);
    g_assert_cmpint(column, ==, 1);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    g_assert_false(isSelected);
    auto cell1 = adoptGRef(atspi_table_get_accessible_at(ATSPI_TABLE(table.get()), 0, 1, nullptr));
    g_assert_true(ATSPI_IS_TABLE_CELL(cell1.get()));
    g_assert_true(cell1.get() == columnHeader1.get());
    g_assert_cmpint(atspi_table_cell_get_row_span(ATSPI_TABLE_CELL(cell1.get()), nullptr), ==, 1);
    g_assert_cmpint(atspi_table_cell_get_column_span(ATSPI_TABLE_CELL(cell1.get()), nullptr), ==, 1);
    g_assert_true(atspi_table_cell_get_position(ATSPI_TABLE_CELL(cell1.get()), &row, &column, nullptr));
    g_assert_cmpint(row, ==, 0);
    g_assert_cmpint(column, ==, 1);
    atspi_table_cell_get_row_column_span(ATSPI_TABLE_CELL(cell1.get()), &row, &column, &rowSpan, &columnSpan, nullptr);
    g_assert_cmpint(row, ==, 0);
    g_assert_cmpint(column, ==, 1);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    cellTable = adoptGRef(atspi_table_cell_get_table(ATSPI_TABLE_CELL(cell1.get()), nullptr));
    g_assert_true(table.get() == cellTable.get());
    rowHeaders = adoptGRef(atspi_table_cell_get_row_header_cells(ATSPI_TABLE_CELL(cell1.get()), nullptr));
    g_assert_cmpint(rowHeaders->len, ==, 1);
    g_assert_true(rowHeaders->pdata[0] == columnHeader0.get());
    g_ptr_array_foreach(rowHeaders.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
    columnHeaders = adoptGRef(atspi_table_cell_get_column_header_cells(ATSPI_TABLE_CELL(cell1.get()), nullptr));
    g_assert_cmpint(columnHeaders->len, ==, 0);
    g_assert_true(ATSPI_IS_TEXT(cell1.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(cell1.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "Column 2");

    g_assert_cmpint(atspi_table_get_index_at(ATSPI_TABLE(table.get()), 0, 2, nullptr), ==, 2);
    g_assert_cmpint(atspi_table_get_row_at_index(ATSPI_TABLE(table.get()), 2, nullptr), ==, 0);
    g_assert_cmpint(atspi_table_get_column_at_index(ATSPI_TABLE(table.get()), 2, nullptr), ==, 2);
    g_assert_cmpint(atspi_table_get_row_extent_at(ATSPI_TABLE(table.get()), 0, 2, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_column_extent_at(ATSPI_TABLE(table.get()), 0, 2, nullptr), ==, 1);
    g_assert_true(atspi_table_get_row_column_extents_at_index(ATSPI_TABLE(table.get()), 2, &row, &column, &rowSpan, &columnSpan, &isSelected, nullptr));
    g_assert_cmpint(row, ==, 0);
    g_assert_cmpint(column, ==, 2);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    g_assert_false(isSelected);
    auto cell2 = adoptGRef(atspi_table_get_accessible_at(ATSPI_TABLE(table.get()), 0, 2, nullptr));
    g_assert_true(ATSPI_IS_TABLE_CELL(cell2.get()));
    g_assert_true(cell2.get() == columnHeader2.get());
    g_assert_cmpint(atspi_table_cell_get_row_span(ATSPI_TABLE_CELL(cell2.get()), nullptr), ==, 1);
    g_assert_cmpint(atspi_table_cell_get_column_span(ATSPI_TABLE_CELL(cell2.get()), nullptr), ==, 1);
    g_assert_true(atspi_table_cell_get_position(ATSPI_TABLE_CELL(cell2.get()), &row, &column, nullptr));
    g_assert_cmpint(row, ==, 0);
    g_assert_cmpint(column, ==, 2);
    atspi_table_cell_get_row_column_span(ATSPI_TABLE_CELL(cell2.get()), &row, &column, &rowSpan, &columnSpan, nullptr);
    g_assert_cmpint(row, ==, 0);
    g_assert_cmpint(column, ==, 2);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    cellTable = adoptGRef(atspi_table_cell_get_table(ATSPI_TABLE_CELL(cell2.get()), nullptr));
    g_assert_true(table.get() == cellTable.get());
    rowHeaders = adoptGRef(atspi_table_cell_get_row_header_cells(ATSPI_TABLE_CELL(cell2.get()), nullptr));
    g_assert_cmpint(rowHeaders->len, ==, 1);
    g_assert_true(rowHeaders->pdata[0] == columnHeader0.get());
    g_ptr_array_foreach(rowHeaders.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
    columnHeaders = adoptGRef(atspi_table_cell_get_column_header_cells(ATSPI_TABLE_CELL(cell2.get()), nullptr));
    g_assert_cmpint(columnHeaders->len, ==, 0);
    g_assert_true(ATSPI_IS_TEXT(cell2.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(cell2.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "Column 3");

    g_assert_cmpint(atspi_table_get_index_at(ATSPI_TABLE(table.get()), 1, 0, nullptr), ==, 3);
    g_assert_cmpint(atspi_table_get_row_at_index(ATSPI_TABLE(table.get()), 3, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_column_at_index(ATSPI_TABLE(table.get()), 3, nullptr), ==, 0);
    g_assert_cmpint(atspi_table_get_row_extent_at(ATSPI_TABLE(table.get()), 1, 0, nullptr), ==, 2);
    g_assert_cmpint(atspi_table_get_column_extent_at(ATSPI_TABLE(table.get()), 1, 0, nullptr), ==, 1);
    g_assert_true(atspi_table_get_row_column_extents_at_index(ATSPI_TABLE(table.get()), 3, &row, &column, &rowSpan, &columnSpan, &isSelected, nullptr));
    g_assert_cmpint(row, ==, 1);
    g_assert_cmpint(column, ==, 0);
    g_assert_cmpint(rowSpan, ==, 2);
    g_assert_cmpint(columnSpan, ==, 1);
    g_assert_false(isSelected);
    auto cell3 = adoptGRef(atspi_table_get_accessible_at(ATSPI_TABLE(table.get()), 1, 0, nullptr));
    g_assert_true(ATSPI_IS_TABLE_CELL(cell3.get()));
    g_assert_true(cell3.get() == rowHeader.get());
    g_assert_cmpint(atspi_table_cell_get_row_span(ATSPI_TABLE_CELL(cell3.get()), nullptr), ==, 2);
    g_assert_cmpint(atspi_table_cell_get_column_span(ATSPI_TABLE_CELL(cell3.get()), nullptr), ==, 1);
    g_assert_true(atspi_table_cell_get_position(ATSPI_TABLE_CELL(cell3.get()), &row, &column, nullptr));
    g_assert_cmpint(row, ==, 1);
    g_assert_cmpint(column, ==, 0);
    atspi_table_cell_get_row_column_span(ATSPI_TABLE_CELL(cell3.get()), &row, &column, &rowSpan, &columnSpan, nullptr);
    g_assert_cmpint(row, ==, 1);
    g_assert_cmpint(column, ==, 0);
    g_assert_cmpint(rowSpan, ==, 2);
    g_assert_cmpint(columnSpan, ==, 1);
    cellTable = adoptGRef(atspi_table_cell_get_table(ATSPI_TABLE_CELL(cell3.get()), nullptr));
    g_assert_true(table.get() == cellTable.get());
    rowHeaders = adoptGRef(atspi_table_cell_get_row_header_cells(ATSPI_TABLE_CELL(cell3.get()), nullptr));
    g_assert_cmpint(rowHeaders->len, ==, 0);
    columnHeaders = adoptGRef(atspi_table_cell_get_column_header_cells(ATSPI_TABLE_CELL(cell3.get()), nullptr));
    g_assert_cmpint(columnHeaders->len, ==, 1);
    g_assert_true(columnHeaders->pdata[0] == columnHeader0.get());
    g_ptr_array_foreach(columnHeaders.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
    g_assert_true(ATSPI_IS_TEXT(cell3.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(cell3.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "Row 1 Cell 1");

    g_assert_cmpint(atspi_table_get_index_at(ATSPI_TABLE(table.get()), 1, 1, nullptr), ==, 4);
    g_assert_cmpint(atspi_table_get_row_at_index(ATSPI_TABLE(table.get()), 4, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_column_at_index(ATSPI_TABLE(table.get()), 4, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_row_extent_at(ATSPI_TABLE(table.get()), 1, 1, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_column_extent_at(ATSPI_TABLE(table.get()), 1, 1, nullptr), ==, 1);
    g_assert_true(atspi_table_get_row_column_extents_at_index(ATSPI_TABLE(table.get()), 4, &row, &column, &rowSpan, &columnSpan, &isSelected, nullptr));
    g_assert_cmpint(row, ==, 1);
    g_assert_cmpint(column, ==, 1);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    g_assert_false(isSelected);
    auto cell4 = adoptGRef(atspi_table_get_accessible_at(ATSPI_TABLE(table.get()), 1, 1, nullptr));
    g_assert_true(ATSPI_IS_TABLE_CELL(cell4.get()));
    g_assert_cmpint(atspi_table_cell_get_row_span(ATSPI_TABLE_CELL(cell4.get()), nullptr), ==, 1);
    g_assert_cmpint(atspi_table_cell_get_column_span(ATSPI_TABLE_CELL(cell4.get()), nullptr), ==, 1);
    g_assert_true(atspi_table_cell_get_position(ATSPI_TABLE_CELL(cell4.get()), &row, &column, nullptr));
    g_assert_cmpint(row, ==, 1);
    g_assert_cmpint(column, ==, 1);
    atspi_table_cell_get_row_column_span(ATSPI_TABLE_CELL(cell4.get()), &row, &column, &rowSpan, &columnSpan, nullptr);
    g_assert_cmpint(row, ==, 1);
    g_assert_cmpint(column, ==, 1);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    cellTable = adoptGRef(atspi_table_cell_get_table(ATSPI_TABLE_CELL(cell4.get()), nullptr));
    g_assert_true(table.get() == cellTable.get());
    rowHeaders = adoptGRef(atspi_table_cell_get_row_header_cells(ATSPI_TABLE_CELL(cell4.get()), nullptr));
    g_assert_cmpint(rowHeaders->len, ==, 1);
    g_assert_true(rowHeaders->pdata[0] == rowHeader.get());
    g_ptr_array_foreach(rowHeaders.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
    columnHeaders = adoptGRef(atspi_table_cell_get_column_header_cells(ATSPI_TABLE_CELL(cell4.get()), nullptr));
    g_assert_cmpint(columnHeaders->len, ==, 1);
    g_assert_true(columnHeaders->pdata[0] == columnHeader1.get());
    g_ptr_array_foreach(columnHeaders.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
    g_assert_true(ATSPI_IS_TEXT(cell4.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(cell4.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "Row 1 Cell 2");

    g_assert_cmpint(atspi_table_get_index_at(ATSPI_TABLE(table.get()), 1, 2, nullptr), ==, 5);
    g_assert_cmpint(atspi_table_get_row_at_index(ATSPI_TABLE(table.get()), 5, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_column_at_index(ATSPI_TABLE(table.get()), 5, nullptr), ==, 2);
    g_assert_cmpint(atspi_table_get_row_extent_at(ATSPI_TABLE(table.get()), 1, 2, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_column_extent_at(ATSPI_TABLE(table.get()), 1, 2, nullptr), ==, 1);
    g_assert_true(atspi_table_get_row_column_extents_at_index(ATSPI_TABLE(table.get()), 5, &row, &column, &rowSpan, &columnSpan, &isSelected, nullptr));
    g_assert_cmpint(row, ==, 1);
    g_assert_cmpint(column, ==, 2);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    g_assert_false(isSelected);
    auto cell5 = adoptGRef(atspi_table_get_accessible_at(ATSPI_TABLE(table.get()), 1, 2, nullptr));
    g_assert_true(ATSPI_IS_TABLE_CELL(cell5.get()));
    g_assert_cmpint(atspi_table_cell_get_row_span(ATSPI_TABLE_CELL(cell5.get()), nullptr), ==, 1);
    g_assert_cmpint(atspi_table_cell_get_column_span(ATSPI_TABLE_CELL(cell5.get()), nullptr), ==, 1);
    g_assert_true(atspi_table_cell_get_position(ATSPI_TABLE_CELL(cell5.get()), &row, &column, nullptr));
    g_assert_cmpint(row, ==, 1);
    g_assert_cmpint(column, ==, 2);
    atspi_table_cell_get_row_column_span(ATSPI_TABLE_CELL(cell5.get()), &row, &column, &rowSpan, &columnSpan, nullptr);
    g_assert_cmpint(row, ==, 1);
    g_assert_cmpint(column, ==, 2);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    cellTable = adoptGRef(atspi_table_cell_get_table(ATSPI_TABLE_CELL(cell5.get()), nullptr));
    g_assert_true(table.get() == cellTable.get());
    rowHeaders = adoptGRef(atspi_table_cell_get_row_header_cells(ATSPI_TABLE_CELL(cell5.get()), nullptr));
    g_assert_cmpint(rowHeaders->len, ==, 1);
    g_assert_true(rowHeaders->pdata[0] == rowHeader.get());
    g_ptr_array_foreach(rowHeaders.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
    columnHeaders = adoptGRef(atspi_table_cell_get_column_header_cells(ATSPI_TABLE_CELL(cell5.get()), nullptr));
    g_assert_cmpint(columnHeaders->len, ==, 1);
    g_assert_true(columnHeaders->pdata[0] == columnHeader2.get());
    g_ptr_array_foreach(columnHeaders.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
    g_assert_true(ATSPI_IS_TEXT(cell5.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(cell5.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "Row 1 Cell 3");

    g_assert_cmpint(atspi_table_get_index_at(ATSPI_TABLE(table.get()), 2, 0, nullptr), ==, 3);
    g_assert_cmpint(atspi_table_get_row_extent_at(ATSPI_TABLE(table.get()), 2, 0, nullptr), ==, 2);
    g_assert_cmpint(atspi_table_get_column_extent_at(ATSPI_TABLE(table.get()), 2, 0, nullptr), ==, 1);
    auto cell = adoptGRef(atspi_table_get_accessible_at(ATSPI_TABLE(table.get()), 2, 0, nullptr));
    g_assert_true(cell3.get() == cell.get());

    g_assert_cmpint(atspi_table_get_index_at(ATSPI_TABLE(table.get()), 2, 1, nullptr), ==, 6);
    g_assert_cmpint(atspi_table_get_row_at_index(ATSPI_TABLE(table.get()), 6, nullptr), ==, 2);
    g_assert_cmpint(atspi_table_get_column_at_index(ATSPI_TABLE(table.get()), 6, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_row_extent_at(ATSPI_TABLE(table.get()), 2, 1, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_column_extent_at(ATSPI_TABLE(table.get()), 2, 1, nullptr), ==, 1);
    g_assert_true(atspi_table_get_row_column_extents_at_index(ATSPI_TABLE(table.get()), 6, &row, &column, &rowSpan, &columnSpan, &isSelected, nullptr));
    g_assert_cmpint(row, ==, 2);
    g_assert_cmpint(column, ==, 1);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    g_assert_false(isSelected);
    auto cell6 = adoptGRef(atspi_table_get_accessible_at(ATSPI_TABLE(table.get()), 2, 1, nullptr));
    g_assert_true(ATSPI_IS_TABLE_CELL(cell6.get()));
    g_assert_cmpint(atspi_table_cell_get_row_span(ATSPI_TABLE_CELL(cell6.get()), nullptr), ==, 1);
    g_assert_cmpint(atspi_table_cell_get_column_span(ATSPI_TABLE_CELL(cell6.get()), nullptr), ==, 1);
    g_assert_true(atspi_table_cell_get_position(ATSPI_TABLE_CELL(cell6.get()), &row, &column, nullptr));
    g_assert_cmpint(row, ==, 2);
    g_assert_cmpint(column, ==, 1);
    atspi_table_cell_get_row_column_span(ATSPI_TABLE_CELL(cell6.get()), &row, &column, &rowSpan, &columnSpan, nullptr);
    g_assert_cmpint(row, ==, 2);
    g_assert_cmpint(column, ==, 1);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    cellTable = adoptGRef(atspi_table_cell_get_table(ATSPI_TABLE_CELL(cell6.get()), nullptr));
    g_assert_true(table.get() == cellTable.get());
    rowHeaders = adoptGRef(atspi_table_cell_get_row_header_cells(ATSPI_TABLE_CELL(cell6.get()), nullptr));
    g_assert_cmpint(rowHeaders->len, ==, 1);
    g_assert_true(rowHeaders->pdata[0] == rowHeader.get());
    g_ptr_array_foreach(rowHeaders.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
    columnHeaders = adoptGRef(atspi_table_cell_get_column_header_cells(ATSPI_TABLE_CELL(cell6.get()), nullptr));
    g_assert_cmpint(columnHeaders->len, ==, 1);
    g_assert_true(columnHeaders->pdata[0] == columnHeader1.get());
    g_ptr_array_foreach(columnHeaders.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
    g_assert_true(ATSPI_IS_TEXT(cell6.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(cell6.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "Row 2 Cell 2");

    g_assert_cmpint(atspi_table_get_index_at(ATSPI_TABLE(table.get()), 2, 2, nullptr), ==, 7);
    g_assert_cmpint(atspi_table_get_row_at_index(ATSPI_TABLE(table.get()), 7, nullptr), ==, 2);
    g_assert_cmpint(atspi_table_get_column_at_index(ATSPI_TABLE(table.get()), 7, nullptr), ==, 2);
    g_assert_cmpint(atspi_table_get_row_extent_at(ATSPI_TABLE(table.get()), 2, 2, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_column_extent_at(ATSPI_TABLE(table.get()), 2, 2, nullptr), ==, 1);
    g_assert_true(atspi_table_get_row_column_extents_at_index(ATSPI_TABLE(table.get()), 7, &row, &column, &rowSpan, &columnSpan, &isSelected, nullptr));
    g_assert_cmpint(row, ==, 2);
    g_assert_cmpint(column, ==, 2);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    g_assert_false(isSelected);
    auto cell7 = adoptGRef(atspi_table_get_accessible_at(ATSPI_TABLE(table.get()), 2, 2, nullptr));
    g_assert_true(ATSPI_IS_TABLE_CELL(cell7.get()));
    g_assert_cmpint(atspi_table_cell_get_row_span(ATSPI_TABLE_CELL(cell7.get()), nullptr), ==, 1);
    g_assert_cmpint(atspi_table_cell_get_column_span(ATSPI_TABLE_CELL(cell7.get()), nullptr), ==, 1);
    g_assert_true(atspi_table_cell_get_position(ATSPI_TABLE_CELL(cell7.get()), &row, &column, nullptr));
    g_assert_cmpint(row, ==, 2);
    g_assert_cmpint(column, ==, 2);
    atspi_table_cell_get_row_column_span(ATSPI_TABLE_CELL(cell7.get()), &row, &column, &rowSpan, &columnSpan, nullptr);
    g_assert_cmpint(row, ==, 2);
    g_assert_cmpint(column, ==, 2);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 1);
    cellTable = adoptGRef(atspi_table_cell_get_table(ATSPI_TABLE_CELL(cell7.get()), nullptr));
    g_assert_true(table.get() == cellTable.get());
    rowHeaders = adoptGRef(atspi_table_cell_get_row_header_cells(ATSPI_TABLE_CELL(cell7.get()), nullptr));
    g_assert_cmpint(rowHeaders->len, ==, 1);
    g_assert_true(rowHeaders->pdata[0] == rowHeader.get());
    g_ptr_array_foreach(rowHeaders.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
    columnHeaders = adoptGRef(atspi_table_cell_get_column_header_cells(ATSPI_TABLE_CELL(cell7.get()), nullptr));
    g_assert_cmpint(columnHeaders->len, ==, 1);
    g_assert_true(columnHeaders->pdata[0] == columnHeader2.get());
    g_ptr_array_foreach(columnHeaders.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
    g_assert_true(ATSPI_IS_TEXT(cell7.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(cell7.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "Row 2 Cell 3");

    g_assert_cmpint(atspi_table_get_index_at(ATSPI_TABLE(table.get()), 3, 0, nullptr), ==, 8);
    g_assert_cmpint(atspi_table_get_row_at_index(ATSPI_TABLE(table.get()), 8, nullptr), ==, 3);
    g_assert_cmpint(atspi_table_get_column_at_index(ATSPI_TABLE(table.get()), 8, nullptr), ==, 0);
    g_assert_cmpint(atspi_table_get_row_extent_at(ATSPI_TABLE(table.get()), 3, 0, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_column_extent_at(ATSPI_TABLE(table.get()), 3, 0, nullptr), ==, 3);
    g_assert_true(atspi_table_get_row_column_extents_at_index(ATSPI_TABLE(table.get()), 8, &row, &column, &rowSpan, &columnSpan, &isSelected, nullptr));
    g_assert_cmpint(row, ==, 3);
    g_assert_cmpint(column, ==, 0);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 3);
    g_assert_false(isSelected);
    auto cell8 = adoptGRef(atspi_table_get_accessible_at(ATSPI_TABLE(table.get()), 3, 0, nullptr));
    g_assert_true(ATSPI_IS_TABLE_CELL(cell8.get()));
    g_assert_cmpint(atspi_table_cell_get_row_span(ATSPI_TABLE_CELL(cell8.get()), nullptr), ==, 1);
    g_assert_cmpint(atspi_table_cell_get_column_span(ATSPI_TABLE_CELL(cell8.get()), nullptr), ==, 3);
    g_assert_true(atspi_table_cell_get_position(ATSPI_TABLE_CELL(cell8.get()), &row, &column, nullptr));
    g_assert_cmpint(row, ==, 3);
    g_assert_cmpint(column, ==, 0);
    atspi_table_cell_get_row_column_span(ATSPI_TABLE_CELL(cell8.get()), &row, &column, &rowSpan, &columnSpan, nullptr);
    g_assert_cmpint(row, ==, 3);
    g_assert_cmpint(column, ==, 0);
    g_assert_cmpint(rowSpan, ==, 1);
    g_assert_cmpint(columnSpan, ==, 3);
    cellTable = adoptGRef(atspi_table_cell_get_table(ATSPI_TABLE_CELL(cell8.get()), nullptr));
    g_assert_true(table.get() == cellTable.get());
    rowHeaders = adoptGRef(atspi_table_cell_get_row_header_cells(ATSPI_TABLE_CELL(cell8.get()), nullptr));
    g_assert_cmpint(rowHeaders->len, ==, 0);
    g_ptr_array_foreach(rowHeaders.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
    columnHeaders = adoptGRef(atspi_table_cell_get_column_header_cells(ATSPI_TABLE_CELL(cell8.get()), nullptr));
    g_assert_cmpint(columnHeaders->len, ==, 1);
    g_assert_true(columnHeaders->pdata[0] == columnHeader0.get());
    g_ptr_array_foreach(columnHeaders.get(), reinterpret_cast<GFunc>(reinterpret_cast<GCallback>(g_object_unref)), nullptr);
    g_assert_true(ATSPI_IS_TEXT(cell8.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(cell8.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "Row 3 Cell 1");

    g_assert_cmpint(atspi_table_get_index_at(ATSPI_TABLE(table.get()), 3, 1, nullptr), ==, 8);
    g_assert_cmpint(atspi_table_get_row_extent_at(ATSPI_TABLE(table.get()), 3, 1, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_column_extent_at(ATSPI_TABLE(table.get()), 3, 1, nullptr), ==, 3);
    cell = adoptGRef(atspi_table_get_accessible_at(ATSPI_TABLE(table.get()), 3, 1, nullptr));
    g_assert_true(cell8.get() == cell.get());

    g_assert_cmpint(atspi_table_get_index_at(ATSPI_TABLE(table.get()), 3, 2, nullptr), ==, 8);
    g_assert_cmpint(atspi_table_get_row_extent_at(ATSPI_TABLE(table.get()), 3, 2, nullptr), ==, 1);
    g_assert_cmpint(atspi_table_get_column_extent_at(ATSPI_TABLE(table.get()), 3, 2, nullptr), ==, 3);
    cell = adoptGRef(atspi_table_get_accessible_at(ATSPI_TABLE(table.get()), 3, 2, nullptr));
    g_assert_true(cell8.get() == cell.get());
}

static void testCollectionGetMatches(AccessibilityTest* test, gconstpointer)
{
    test->showInWindow(800, 600);
    test->loadHtml(
        "<html>"
        "  <body>"
        "    <h1>Collection</h1>"
        "    <p>This is <a href='#'>link1</a> and <a href='#'>link2</a> in paragraph</p>"
        "  </body>"
        "</html>",
        nullptr);
    test->waitUntilLoadFinished();

    auto testApp = test->findTestApplication();
    g_assert_true(ATSPI_IS_ACCESSIBLE(testApp.get()));

    auto documentWeb = test->findDocumentWeb(testApp.get());
    g_assert_true(ATSPI_IS_ACCESSIBLE(documentWeb.get()));
    g_assert_true(ATSPI_IS_COLLECTION(documentWeb.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(documentWeb.get(), nullptr), ==, 2);

    auto h1 = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(h1.get()));

    auto p = adoptGRef(atspi_accessible_get_child_at_index(documentWeb.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(p.get()));
    g_assert_cmpint(atspi_accessible_get_child_count(p.get(), nullptr), ==, 2);

    auto link1 = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 0, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(link1.get()));
    GUniquePtr<char> text(atspi_text_get_text(ATSPI_TEXT(link1.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "link1");

    auto link2 = adoptGRef(atspi_accessible_get_child_at_index(p.get(), 1, nullptr));
    g_assert_true(ATSPI_IS_ACCESSIBLE(link2.get()));
    text.reset(atspi_text_get_text(ATSPI_TEXT(link2.get()), 0, -1, nullptr));
    g_assert_cmpstr(text.get(), ==, "link2");

    // A simple rule based on roles.
    GArray* roles = g_array_sized_new(FALSE, FALSE, sizeof(int), 1);
    int linkRole = ATSPI_ROLE_LINK;
    g_array_prepend_val(roles, linkRole);
    GRefPtr<AtspiMatchRule> rule = adoptGRef(atspi_match_rule_new(
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        roles, ATSPI_Collection_MATCH_ALL,
        nullptr, ATSPI_Collection_MATCH_NONE,
        FALSE));
    g_array_free(roles, TRUE);
    GArray* matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 2);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == link1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 1) == link2.get());
    g_array_free(matches, TRUE);

    // Reverse order.
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_REVERSE_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 2);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == link2.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 1) == link1.get());
    g_array_free(matches, TRUE);

    // Limit results to 1.
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 1, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == link1.get());
    g_array_free(matches, TRUE);

    // Don't traverse.
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, FALSE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 0);
    g_array_free(matches, TRUE);

    matches = atspi_collection_get_matches(ATSPI_COLLECTION(p.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, FALSE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 2);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == link1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 1) == link2.get());
    g_array_free(matches, TRUE);

    // Rule to match all interfaces.
    GArray* interfaces = g_array_sized_new(FALSE, FALSE, sizeof(char*), 3);
    static const char* linkInterface = "hyperlink";
    static const char* textInterface = "text";
    g_array_append_val(interfaces, linkInterface);
    g_array_append_val(interfaces, textInterface);
    rule = adoptGRef(atspi_match_rule_new(
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        interfaces, ATSPI_Collection_MATCH_ALL,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 2);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == link1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 1) == link2.get());
    g_array_free(matches, TRUE);

    // Adding table interface to make the match fail.
    static const char* tableInterface = "table";
    g_array_append_val(interfaces, tableInterface);
    rule = adoptGRef(atspi_match_rule_new(
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        interfaces, ATSPI_Collection_MATCH_ALL,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 0);
    g_array_free(matches, TRUE);

    // Rule to match any of the interfaces.
    rule = adoptGRef(atspi_match_rule_new(
	nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        interfaces, ATSPI_Collection_MATCH_ANY,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 4);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == h1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 1) == p.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 2) == link1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 3) == link2.get());
    g_array_free(matches, TRUE);

    // Rule to match none of the interfaces.
    g_array_remove_range(interfaces, 1, 2);
    rule = adoptGRef(atspi_match_rule_new(
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        interfaces, ATSPI_Collection_MATCH_NONE,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 2);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == h1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 1) == p.get());
    g_array_free(matches, TRUE);
    g_array_free(interfaces, TRUE);

    // Rule to match all states.
    GRefPtr<AtspiStateSet> set = adoptGRef(atspi_state_set_new(nullptr));
    atspi_state_set_add(set.get(), ATSPI_STATE_SHOWING);
    atspi_state_set_add(set.get(), ATSPI_STATE_FOCUSABLE);
    rule = adoptGRef(atspi_match_rule_new(
        set.get(), ATSPI_Collection_MATCH_ALL,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 2);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == link1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 1) == link2.get());
    g_array_free(matches, TRUE);

    // Adding checkable state to make the match fail.
    atspi_state_set_add(set.get(), ATSPI_STATE_CHECKABLE);
    rule = adoptGRef(atspi_match_rule_new(
        set.get(), ATSPI_Collection_MATCH_ALL,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 0);
    g_array_free(matches, TRUE);

    // Rule to match any of the states.
    rule = adoptGRef(atspi_match_rule_new(
        set.get(), ATSPI_Collection_MATCH_ANY,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 4);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == h1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 1) == p.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 2) == link1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 3) == link2.get());
    g_array_free(matches, TRUE);

    // Rule to match none of the states.
    atspi_state_set_remove(set.get(), ATSPI_STATE_CHECKABLE);
    atspi_state_set_remove(set.get(), ATSPI_STATE_SHOWING);
    rule = adoptGRef(atspi_match_rule_new(
        set.get(), ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 2);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == h1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 1) == p.get());
    g_array_free(matches, TRUE);

    // Rule to match all roles.
    roles = g_array_sized_new(FALSE, FALSE, sizeof(int), 2);
    int headingRole = ATSPI_ROLE_HEADING;
    g_array_prepend_val(roles, linkRole);
    g_array_prepend_val(roles, headingRole);
    rule = adoptGRef(atspi_match_rule_new(
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        roles, ATSPI_Collection_MATCH_ALL,
        nullptr, ATSPI_Collection_MATCH_NONE,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    // No matches when more than one role is given for MATCH_ALL.
    g_assert_cmpuint(matches->len, ==, 0);
    g_array_free(matches, TRUE);

    // Rule to match any of the roles.
    rule = adoptGRef(atspi_match_rule_new(
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        roles, ATSPI_Collection_MATCH_ANY,
        nullptr, ATSPI_Collection_MATCH_NONE,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 3);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == h1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 1) == link1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 2) == link2.get());
    g_array_free(matches, TRUE);

    // Rule to match none of the roles.
    rule = adoptGRef(atspi_match_rule_new(
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        roles, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == p.get());
    g_array_free(matches, TRUE);

    g_array_free(roles, TRUE);

    // Rule to match all attributes.
    GRefPtr<GHashTable> attributes = adoptGRef(g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free));
    g_hash_table_insert(attributes.get(), g_strdup("tag"), g_strdup("a"));
    rule = adoptGRef(atspi_match_rule_new(
        nullptr, ATSPI_Collection_MATCH_NONE,
        attributes.get(), ATSPI_Collection_MATCH_ALL,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 2);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == link1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 1) == link2.get());
    g_array_free(matches, TRUE);

    // Adding level attribute to make the match fail.
    g_hash_table_insert(attributes.get(), g_strdup("level"), g_strdup("1"));
    rule = adoptGRef(atspi_match_rule_new(
        nullptr, ATSPI_Collection_MATCH_NONE,
        attributes.get(), ATSPI_Collection_MATCH_ALL,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 0);
    g_array_free(matches, TRUE);

    // Rule to match any of the attributes.
    g_hash_table_insert(attributes.get(), g_strdup("level"), g_strdup("1:2:3:4"));
    rule = adoptGRef(atspi_match_rule_new(
        nullptr, ATSPI_Collection_MATCH_NONE,
        attributes.get(), ATSPI_Collection_MATCH_ANY,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 3);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == h1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 1) == link1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 2) == link2.get());
    g_array_free(matches, TRUE);

    // Rule to match none of the attributes.
    rule = adoptGRef(atspi_match_rule_new(
        nullptr, ATSPI_Collection_MATCH_NONE,
        attributes.get(), ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        FALSE));
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 1);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == p.get());
    g_array_free(matches, TRUE);

    // Combined rule to find any focusable elements that implement text.
    interfaces = g_array_sized_new(FALSE, FALSE, sizeof(char*), 1);
    g_array_append_val(interfaces, textInterface);
    set = adoptGRef(atspi_state_set_new(nullptr));
    atspi_state_set_add(set.get(), ATSPI_STATE_FOCUSABLE);
    rule = adoptGRef(atspi_match_rule_new(
        set.get(), ATSPI_Collection_MATCH_ALL,
        nullptr, ATSPI_Collection_MATCH_NONE,
        nullptr, ATSPI_Collection_MATCH_NONE,
        interfaces, ATSPI_Collection_MATCH_ALL,
        FALSE));
    g_array_free(interfaces, TRUE);
    matches = atspi_collection_get_matches(ATSPI_COLLECTION(documentWeb.get()), rule.get(), ATSPI_Collection_SORT_ORDER_CANONICAL, 0, TRUE, nullptr);
    g_assert_nonnull(matches);
    g_assert_cmpuint(matches->len, ==, 2);
    g_assert_true(g_array_index(matches, AtspiAccessible*, 0) == link1.get());
    g_assert_true(g_array_index(matches, AtspiAccessible*, 1) == link2.get());
    g_array_free(matches, TRUE);
}

void beforeAll()
{
    AccessibilityTest::add("WebKitAccessibility", "accessible/basic-hierarchy", testAccessibleBasicHierarchy);
    AccessibilityTest::add("WebKitAccessibility", "accessible/ignored-objects", testAccessibleIgnoredObjects);
    AccessibilityTest::add("WebKitAccessibility", "accessible/children-changed", testAccessibleChildrenChanged);
    AccessibilityTest::add("WebKitAccessibility", "accessible/attributes", testAccessibleAttributes);
    AccessibilityTest::add("WebKitAccessibility", "accessible/state", testAccessibleState);
    AccessibilityTest::add("WebKitAccessibility", "accessible/state-changed", testAccessibleStateChanged);
    AccessibilityTest::add("WebKitAccessibility", "accessible/event-listener", testAccessibleEventListener);
    AccessibilityTest::add("WebKitAccessibility", "accessible/list-markers", testAccessibleListMarkers);
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
    AccessibilityTest::add("WebKitAccessibility", "text/replaced-objects", testTextReplacedObjects);
    AccessibilityTest::add("WebKitAccessibility", "text/list-markers", testTextListMarkers);
    AccessibilityTest::add("WebKitAccessibility", "value/basic", testValueBasic);
    AccessibilityTest::add("WebKitAccessibility", "hyperlink/basic", testHyperlinkBasic);
    AccessibilityTest::add("WebKitAccessibility", "hypertext/basic", testHypertextBasic);
    AccessibilityTest::add("WebKitAccessibility", "action/basic", testActionBasic);
    AccessibilityTest::add("WebKitAccessibility", "document/basic", testDocumentBasic);
    AccessibilityTest::add("WebKitAccessibility", "document/load-events", testDocumentLoadEvents);
    AccessibilityTest::add("WebKitAccessibility", "image/basic", testImageBasic);
    AccessibilityTest::add("WebKitAccessibility", "selection/listbox", testSelectionListBox);
    AccessibilityTest::add("WebKitAccessibility", "selection/menulist", testSelectionMenuList);
    AccessibilityTest::add("WebKitAccessibility", "table/basic", testTableBasic);
    AccessibilityTest::add("WebKitAccessibility", "collection/get-matches", testCollectionGetMatches);
}

void afterAll()
{
}
