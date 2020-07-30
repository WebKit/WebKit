/*
 * Copyright (C) 2017 Igalia S.L.
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
#include <wtf/glib/GUniquePtr.h>

#if PLATFORM(GTK)
#include <WebCore/GtkVersioning.h>
#if USE(GTK4)
#include "WebKitWebViewBaseInternal.h"
using PlatformEventKey = GdkEvent;
#else
using PlatformEventKey = GdkEventKey;
#endif
#define KEY(x) GDK_KEY_##x
#define CONTROL_MASK GDK_CONTROL_MASK
#define SHIFT_MASK GDK_SHIFT_MASK
#elif PLATFORM(WPE)
using PlatformEventKey = struct wpe_input_keyboard_event;
#define KEY(x) WPE_KEY_##x
#define CONTROL_MASK wpe_input_keyboard_modifier_control
#define SHIFT_MASK wpe_input_keyboard_modifier_shift
#endif

typedef struct _WebKitInputMethodContextMock {
    WebKitInputMethodContext parent;

    bool enabled;
    GString* preedit;
    bool commitNextCharacter;
    char* surroundingText;
    unsigned surroundingCursorIndex;
    unsigned surroundingSelectionIndex;
} WebKitInputMethodContextMock;

typedef struct _WebKitInputMethodContextMockClass {
    WebKitInputMethodContextClass parent;
} WebKitInputMethodContextMockClass;

G_DEFINE_TYPE(WebKitInputMethodContextMock, webkit_input_method_context_mock, WEBKIT_TYPE_INPUT_METHOD_CONTEXT)

static const char* testHTML = "<html><body><textarea id='editable' rows='3', cols='50' onkeydown='logKeyDown()' onkeyup='logKeyUp()' onkeypress='logKeyPress()'></textarea><script>"
    "var input = document.getElementById('editable');"
    "input.addEventListener('compositionstart', logCompositionEvent);"
    "input.addEventListener('compositionupdate', logCompositionEvent);"
    "input.addEventListener('compositionend', logCompositionEvent);"
    "function logCompositionEvent(event) { window.webkit.messageHandlers.imEvent.postMessage({ 'type' : event.type, 'data' : event.data }) }"
    "function logKeyDown() { window.webkit.messageHandlers.imEvent.postMessage({ 'type' : 'keyDown', 'keyCode' : event.keyCode, 'key' : event.key, 'isComposing' : event.isComposing }) }"
    "function logKeyUp() { window.webkit.messageHandlers.imEvent.postMessage({ 'type' : 'keyUp', 'keyCode' : event.keyCode, 'key' : event.key, 'isComposing' : event.isComposing }) }"
    "function logKeyPress() { window.webkit.messageHandlers.imEvent.postMessage({ 'type' : 'keyPress', 'keyCode' : event.keyCode }) }"
    "</script></body></html>";

static void webkitInputMethodContextMockFinalize(GObject* object)
{
    auto* mock = reinterpret_cast<WebKitInputMethodContextMock*>(object);
    if (mock->preedit) {
        g_string_free(mock->preedit, TRUE);
        mock->preedit = nullptr;
    }
    g_clear_pointer(&mock->surroundingText, g_free);
    G_OBJECT_CLASS(webkit_input_method_context_mock_parent_class)->finalize(object);
}

static void webkitInputMethodContextMockGetPreedit(WebKitInputMethodContext* context, char** text, GList** underlines, guint* cursorOffset)
{
    auto* mock = reinterpret_cast<WebKitInputMethodContextMock*>(context);
    if (text)
        *text = mock->preedit ? g_strdup(mock->preedit->str) : g_strdup("");
    if (underlines)
        *underlines = mock->preedit ? g_list_prepend(*underlines, webkit_input_method_underline_new(0, mock->preedit->len)) : nullptr;
    if (cursorOffset)
        *cursorOffset = mock->preedit ? mock->preedit->len : 0;
}

static gboolean webkitInputMethodContextMockFilterKeyEvent(WebKitInputMethodContext* context, PlatformEventKey* keyEvent)
{
    auto* mock = reinterpret_cast<WebKitInputMethodContextMock*>(context);
    if (!mock->enabled)
        return FALSE;

#if PLATFORM(GTK)
#if USE(GTK4)
    auto* event = reinterpret_cast<KeyEvent*>(keyEvent);
    unsigned keyval = event->keyval;
    auto state = static_cast<GdkModifierType>(event->modifiers);
    bool isKeyPress = event->type == GDK_KEY_PRESS;
#else
    GdkModifierType state;
    guint keyval;
    if (!gdk_event_get_state(reinterpret_cast<GdkEvent*>(keyEvent), &state) || !gdk_event_get_keyval(reinterpret_cast<GdkEvent*>(keyEvent), &keyval))
        return FALSE;
    bool isKeyPress = gdk_event_get_event_type(reinterpret_cast<GdkEvent*>(keyEvent)) == GDK_KEY_PRESS;
#endif
    gunichar character = gdk_keyval_to_unicode(keyval);
#elif PLATFORM(WPE)
    uint32_t state = keyEvent->modifiers;
    uint32_t keyval = keyEvent->key_code;
    bool isKeyPress = keyEvent->pressed;
    gunichar character = wpe_key_code_to_unicode(keyval);
#endif
    bool isControl = state & CONTROL_MASK;
    bool isShift = state & SHIFT_MASK;
    bool isComposeEnd = (keyval == KEY(space) || keyval == KEY(Return) || keyval == KEY(ISO_Enter));

    if (isKeyPress && mock->commitNextCharacter) {
        char buffer[6];
        auto length = g_unichar_to_utf8(character, buffer);
        buffer[length] = '\0';
        g_signal_emit_by_name(context, "committed", buffer, nullptr);
        mock->commitNextCharacter = false;

        return TRUE;
    }

    if (!mock->preedit) {
        if (isKeyPress && isControl && isShift && keyval == KEY(w)) {
            mock->preedit = g_string_new("w");
            g_signal_emit_by_name(context, "preedit-started", nullptr);
            g_signal_emit_by_name(context, "preedit-changed", nullptr);

            return TRUE;
        }

        return FALSE;
    }

    if (keyval == KEY(Escape)) {
        g_string_free(mock->preedit, TRUE);
        mock->preedit = nullptr;
        g_signal_emit_by_name(context, "preedit-changed", nullptr);
        g_signal_emit_by_name(context, "preedit-finished", nullptr);

        return TRUE;
    }

    if (isComposeEnd) {
        if (!g_strcmp0(mock->preedit->str, "wgtk"))
            g_signal_emit_by_name(context, "committed", "WebKitGTK", nullptr);
        else if (!g_strcmp0(mock->preedit->str, "wwpe"))
            g_signal_emit_by_name(context, "committed", "WPEWebKit", nullptr);
        else
            g_signal_emit_by_name(context, "committed", mock->preedit->str + 1, nullptr);

        g_string_free(mock->preedit, TRUE);
        mock->preedit = nullptr;
        g_signal_emit_by_name(context, "preedit-changed", nullptr);
        g_signal_emit_by_name(context, "preedit-finished", nullptr);

        return TRUE;
    }

    if (isKeyPress) {
        g_string_append_unichar(mock->preedit, character);
        g_signal_emit_by_name(context, "preedit-changed", nullptr);

        return TRUE;
    }

    return FALSE;
}

static void webkitInputMethodContextMockNotifyFocusIn(WebKitInputMethodContext* context)
{
    reinterpret_cast<WebKitInputMethodContextMock*>(context)->enabled = true;
}

static void webkitInputMethodContextMockNotifyFocusOut(WebKitInputMethodContext* context)
{
    reinterpret_cast<WebKitInputMethodContextMock*>(context)->enabled = false;
}

static void webkitInputMethodContextMockNotifySurrounding(WebKitInputMethodContext* context, const gchar *text, unsigned length, unsigned cursorIndex, unsigned selectionIndex)
{
    auto* mock = reinterpret_cast<WebKitInputMethodContextMock*>(context);
    g_clear_pointer(&mock->surroundingText, g_free);

    if (!mock->preedit && cursorIndex >= 3 && text[cursorIndex - 3] == ':' && text[cursorIndex - 2] == '-' && text[cursorIndex - 1] == ')') {
        g_signal_emit_by_name(context, "delete-surrounding", -3, 3, nullptr);
        g_signal_emit_by_name(context, "committed", "😀️", nullptr);
    }
    mock->surroundingText = g_strndup(text, length);
    mock->surroundingCursorIndex = cursorIndex;
    mock->surroundingSelectionIndex = selectionIndex;
}

static void webkitInputMethodContextMockReset(WebKitInputMethodContext* context)
{
    auto* mock = reinterpret_cast<WebKitInputMethodContextMock*>(context);
    if (!mock->preedit)
        return;

    g_string_free(mock->preedit, TRUE);
    mock->preedit = nullptr;
    g_clear_pointer(&mock->surroundingText, g_free);
    mock->surroundingCursorIndex = 0;
    mock->surroundingSelectionIndex = 0;

    g_signal_emit_by_name(context, "preedit-changed", nullptr);
    g_signal_emit_by_name(context, "preedit-finished", nullptr);
}

static void webkit_input_method_context_mock_class_init(WebKitInputMethodContextMockClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->finalize = webkitInputMethodContextMockFinalize;

    auto* imClass = WEBKIT_INPUT_METHOD_CONTEXT_CLASS(klass);
    imClass->get_preedit = webkitInputMethodContextMockGetPreedit;
    imClass->filter_key_event = webkitInputMethodContextMockFilterKeyEvent;
    imClass->notify_focus_in = webkitInputMethodContextMockNotifyFocusIn;
    imClass->notify_focus_out = webkitInputMethodContextMockNotifyFocusOut;
    imClass->notify_surrounding = webkitInputMethodContextMockNotifySurrounding;
    imClass->reset = webkitInputMethodContextMockReset;
}

static void webkit_input_method_context_mock_init(WebKitInputMethodContextMock*)
{
}

class InputMethodTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(InputMethodTest);

    struct Event {
        enum class Type { KeyDown, KeyPress, KeyUp, CompositionStart, CompositionUpdate, CompositionEnd };

        explicit Event(Type type)
            : type(type)
        {
        }

        Type type;
        CString data;
        unsigned keyCode;
        CString key;
        bool isComposing;
    };

    static void imEventCallback(WebKitUserContentManager*, WebKitJavascriptResult* javascriptResult, InputMethodTest* test)
    {
        test->imEvent(javascriptResult);
    }

    InputMethodTest()
        : m_context(adoptGRef(static_cast<WebKitInputMethodContextMock*>(g_object_new(webkit_input_method_context_mock_get_type(), nullptr))))
    {
        WebViewTest::showInWindow();
#if PLATFORM(GTK)
        auto* defaultContext = webkit_web_view_get_input_method_context(m_webView);
        g_assert_true(WEBKIT_IS_INPUT_METHOD_CONTEXT(defaultContext));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(defaultContext));
#elif PLATFORM(WPE)
        g_assert_null(webkit_web_view_get_input_method_context(m_webView));
#endif
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_context.get()));
        webkit_web_view_set_input_method_context(m_webView, WEBKIT_INPUT_METHOD_CONTEXT(m_context.get()));
        g_assert_true(webkit_web_view_get_input_method_context(m_webView) == WEBKIT_INPUT_METHOD_CONTEXT(m_context.get()));

        webkit_user_content_manager_register_script_message_handler(m_userContentManager.get(), "imEvent");
        g_signal_connect(m_userContentManager.get(), "script-message-received::imEvent", G_CALLBACK(imEventCallback), this);
    }

    ~InputMethodTest()
    {
        webkit_user_content_manager_unregister_script_message_handler(m_userContentManager.get(), "imEvent");
        g_signal_handlers_disconnect_by_data(m_userContentManager.get(), this);
    }

    void imEvent(WebKitJavascriptResult* result)
    {
        auto* jsEvent = webkit_javascript_result_get_js_value(result);
        g_assert_true(jsc_value_is_object(jsEvent));

        GRefPtr<JSCValue> value = adoptGRef(jsc_value_object_get_property(jsEvent, "type"));
        g_assert_true(jsc_value_is_string(value.get()));
        GUniquePtr<char> strValue(jsc_value_to_string(value.get()));
        if (!g_strcmp0(strValue.get(), "keyDown")) {
            InputMethodTest::Event event(InputMethodTest::Event::Type::KeyDown);
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "keyCode"));
            g_assert_true(jsc_value_is_number(value.get()));
            event.keyCode = jsc_value_to_int32(value.get());
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "key"));
            g_assert_true(jsc_value_is_string(value.get()));
            strValue.reset(jsc_value_to_string(value.get()));
            event.key = strValue.get();
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "isComposing"));
            g_assert_true(jsc_value_is_boolean(value.get()));
            event.isComposing = jsc_value_to_boolean(value.get());
            m_events.append(WTFMove(event));
        } else if (!g_strcmp0(strValue.get(), "keyPress")) {
            InputMethodTest::Event event(InputMethodTest::Event::Type::KeyPress);
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "keyCode"));
            g_assert_true(jsc_value_is_number(value.get()));
            event.keyCode = jsc_value_to_int32(value.get());
            m_events.append(WTFMove(event));
        } else if (!g_strcmp0(strValue.get(), "keyUp")) {
            InputMethodTest::Event event(InputMethodTest::Event::Type::KeyUp);
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "keyCode"));
            g_assert_true(jsc_value_is_number(value.get()));
            event.keyCode = jsc_value_to_int32(value.get());
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "key"));
            g_assert_true(jsc_value_is_string(value.get()));
            strValue.reset(jsc_value_to_string(value.get()));
            event.key = strValue.get();
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "isComposing"));
            g_assert_true(jsc_value_is_boolean(value.get()));
            event.isComposing = jsc_value_to_boolean(value.get());
            m_events.append(WTFMove(event));
        } else if (!g_strcmp0(strValue.get(), "compositionstart")) {
            InputMethodTest::Event event(InputMethodTest::Event::Type::CompositionStart);
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "data"));
            g_assert_true(jsc_value_is_string(value.get()));
            strValue.reset(jsc_value_to_string(value.get()));
            event.data = strValue.get();
            m_events.append(WTFMove(event));
        } else if (!g_strcmp0(strValue.get(), "compositionupdate")) {
            InputMethodTest::Event event(InputMethodTest::Event::Type::CompositionUpdate);
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "data"));
            g_assert_true(jsc_value_is_string(value.get()));
            strValue.reset(jsc_value_to_string(value.get()));
            event.data = strValue.get();
            m_events.append(WTFMove(event));
        } else if (!g_strcmp0(strValue.get(), "compositionend")) {
            InputMethodTest::Event event(InputMethodTest::Event::Type::CompositionEnd);
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "data"));
            g_assert_true(jsc_value_is_string(value.get()));
            strValue.reset(jsc_value_to_string(value.get()));
            event.data = strValue.get();
            m_events.append(WTFMove(event));
        }

        if (m_events.size() == m_eventsExpected)
            g_main_loop_quit(m_mainLoop);
    }

    void focusEditableAndWaitUntilInputMethodEnabled()
    {
        g_assert_false(m_context->enabled);
        runJavaScriptAndWaitUntilFinished("document.getElementById('editable').focus()", nullptr);
        if (m_context->enabled)
            return;

        g_idle_add([](gpointer userData) -> gboolean {
            auto* test = static_cast<InputMethodTest*>(userData);
            if (test->m_context->enabled) {
                test->quitMainLoop();
                return FALSE;
            }

            return TRUE;
        }, this);
        g_main_loop_run(m_mainLoop);
        g_assert_true(m_context->enabled);
    }

    void unfocusEditableAndWaitUntilInputMethodDisabled()
    {
        g_assert_true(m_context->enabled);
        runJavaScriptAndWaitUntilFinished("document.getElementById('editable').blur()", nullptr);
        if (!m_context->enabled)
            return;

        g_idle_add([](gpointer userData) -> gboolean {
            auto* test = static_cast<InputMethodTest*>(userData);
            if (!test->m_context->enabled) {
                test->quitMainLoop();
                return FALSE;
            }

            return TRUE;
        }, this);
        g_main_loop_run(m_mainLoop);
        g_assert_false(m_context->enabled);
    }

    void resetEditable()
    {
        runJavaScriptAndWaitUntilFinished("document.getElementById('editable').value = ''", nullptr);
        m_events.clear();
    }

    GUniquePtr<char> editableValue()
    {
        auto* jsResult = runJavaScriptAndWaitUntilFinished("document.getElementById('editable').value", nullptr);
        return GUniquePtr<char>(WebViewTest::javascriptResultToCString(jsResult));
    }

    void keyStrokeAndWaitForEvents(unsigned keyval, unsigned eventsCount, unsigned modifiers = 0)
    {
        m_eventsExpected = eventsCount;
        keyStroke(keyval, modifiers);
        g_main_loop_run(m_mainLoop);
        m_eventsExpected = 0;
    }

    void keyStrokeHandledByInputMethodAndWaitForEvents(unsigned keyval, unsigned eventsCount)
    {
        m_context->commitNextCharacter = true;
        keyStrokeAndWaitForEvents(keyval, eventsCount);
        m_context->commitNextCharacter = false;
    }

    void clickAndWaitForEvents(unsigned eventsCount)
    {
        m_eventsExpected = eventsCount;
        clickMouseButton(0, 0, 1);
        g_main_loop_run(m_mainLoop);
        m_eventsExpected = 0;
    }

    WebKitInputPurpose purpose() const
    {
        return webkit_input_method_context_get_input_purpose(WEBKIT_INPUT_METHOD_CONTEXT(m_context.get()));
    }

    WebKitInputHints hints() const
    {
        return webkit_input_method_context_get_input_hints(WEBKIT_INPUT_METHOD_CONTEXT(m_context.get()));
    }

    const char* surroundingText() const
    {
        return m_context->surroundingText;
    }

    unsigned surroundingCursorIndex() const
    {
        return m_context->surroundingCursorIndex;
    }

    unsigned surroundingSelectionIndex() const
    {
        return m_context->surroundingSelectionIndex;
    }

    void waitForSurroundingText(const char* text)
    {
        m_expectedSurroundingText = text;
        g_idle_add([](gpointer userData) -> gboolean {
            auto* test = static_cast<InputMethodTest*>(userData);
            if (!g_strcmp0(test->m_context->surroundingText, test->m_expectedSurroundingText.data())) {
                test->quitMainLoop();
                return FALSE;
            }

            return TRUE;
        }, this);
        g_main_loop_run(m_mainLoop);
        m_expectedSurroundingText = { };
    }

    GRefPtr<WebKitInputMethodContextMock> m_context;
    Vector<Event> m_events;
    unsigned m_eventsExpected { 0 };
    CString m_expectedSurroundingText;
};

static void testWebKitInputMethodContextSimple(InputMethodTest* test, gconstpointer)
{
    test->loadHtml(testHTML, nullptr);
    test->waitUntilLoadFinished();

    test->focusEditableAndWaitUntilInputMethodEnabled();

    // Send a normal character not handled by IM.
    test->keyStrokeAndWaitForEvents(KEY(a), 3);
    g_assert_cmpuint(test->m_events.size(), ==, 3);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 65);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "a");
    g_assert_false(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::KeyPress);
    g_assert_cmpuint(test->m_events[1].keyCode, ==, 97);
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[2].keyCode, ==, 65);
    g_assert_cmpstr(test->m_events[2].key.data(), ==, "a");
    g_assert_false(test->m_events[2].isComposing);
    {
        auto editableValue = test->editableValue();
        g_assert_cmpstr(editableValue.get(), ==, "a");
    }
    test->resetEditable();

    // Send a normal character handled by IM.
    test->keyStrokeHandledByInputMethodAndWaitForEvents(KEY(a), 3);
    g_assert_cmpuint(test->m_events.size(), ==, 3);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 65);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "a");
    g_assert_false(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::KeyPress);
    g_assert_cmpuint(test->m_events[1].keyCode, ==, 97);
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[2].keyCode, ==, 65);
    g_assert_cmpstr(test->m_events[2].key.data(), ==, "a");
    g_assert_false(test->m_events[2].isComposing);
    {
        auto editableValue = test->editableValue();
        g_assert_cmpstr(editableValue.get(), ==, "a");
    }
    test->resetEditable();
}

static void testWebKitInputMethodContextSequence(InputMethodTest* test, gconstpointer)
{
    test->loadHtml(testHTML, nullptr);
    test->waitUntilLoadFinished();

    test->focusEditableAndWaitUntilInputMethodEnabled();

    // Compose w + gtk + Enter.
    test->keyStrokeAndWaitForEvents(KEY(w), 4, CONTROL_MASK | SHIFT_MASK);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_false(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionStart);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::CompositionUpdate);
    g_assert_cmpstr(test->m_events[2].data.data(), ==, "w");
    g_assert_true(test->m_events[3].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[3].keyCode, ==, 87);
    g_assert_cmpstr(test->m_events[3].key.data(), ==, "w");
    g_assert_true(test->m_events[3].isComposing);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(g), 3);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_true(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionUpdate);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "wg");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[2].keyCode, ==, 71);
    g_assert_cmpstr(test->m_events[2].key.data(), ==, "g");
    g_assert_true(test->m_events[2].isComposing);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(t), 3);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_true(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionUpdate);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "wgt");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[2].keyCode, ==, 84);
    g_assert_cmpstr(test->m_events[2].key.data(), ==, "t");
    g_assert_true(test->m_events[2].isComposing);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(k), 3);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_true(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionUpdate);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "wgtk");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[2].keyCode, ==, 75);
    g_assert_cmpstr(test->m_events[2].key.data(), ==, "k");
    g_assert_true(test->m_events[2].isComposing);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(ISO_Enter), 3);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_true(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionEnd);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "WebKitGTK");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[2].keyCode, ==, 13);
    g_assert_cmpstr(test->m_events[2].key.data(), ==, "Enter");
    g_assert_false(test->m_events[2].isComposing);
    {
        auto editableValue = test->editableValue();
        g_assert_cmpstr(editableValue.get(), ==, "WebKitGTK");
    }
    test->resetEditable();

    // Compose w + wpe + Space.
    test->keyStrokeAndWaitForEvents(KEY(w), 4, CONTROL_MASK | SHIFT_MASK);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_false(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionStart);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::CompositionUpdate);
    g_assert_cmpstr(test->m_events[2].data.data(), ==, "w");
    g_assert_true(test->m_events[3].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[3].keyCode, ==, 87);
    g_assert_cmpstr(test->m_events[3].key.data(), ==, "w");
    g_assert_true(test->m_events[3].isComposing);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(w), 3);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_true(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionUpdate);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "ww");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[2].keyCode, ==, 87);
    g_assert_cmpstr(test->m_events[2].key.data(), ==, "w");
    g_assert_true(test->m_events[2].isComposing);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(p), 3);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_true(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionUpdate);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "wwp");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[2].keyCode, ==, 80);
    g_assert_cmpstr(test->m_events[2].key.data(), ==, "p");
    g_assert_true(test->m_events[2].isComposing);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(e), 3);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_true(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionUpdate);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "wwpe");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[2].keyCode, ==, 69);
    g_assert_cmpstr(test->m_events[2].key.data(), ==, "e");
    g_assert_true(test->m_events[2].isComposing);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(space), 3);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_true(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionEnd);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "WPEWebKit");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[2].keyCode, ==, 32);
    g_assert_cmpstr(test->m_events[2].key.data(), ==, " ");
    g_assert_false(test->m_events[2].isComposing);
    {
        auto editableValue = test->editableValue();
        g_assert_cmpstr(editableValue.get(), ==, "WPEWebKit");
    }
    test->resetEditable();
}

static void testWebKitInputMethodContextInvalidSequence(InputMethodTest* test, gconstpointer)
{
    test->loadHtml(testHTML, nullptr);
    test->waitUntilLoadFinished();

    test->focusEditableAndWaitUntilInputMethodEnabled();

    // Compose w + w + Space -> invalid sequence.
    test->keyStrokeAndWaitForEvents(KEY(w), 4, CONTROL_MASK | SHIFT_MASK);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_false(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionStart);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::CompositionUpdate);
    g_assert_cmpstr(test->m_events[2].data.data(), ==, "w");
    g_assert_true(test->m_events[3].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[3].keyCode, ==, 87);
    g_assert_cmpstr(test->m_events[3].key.data(), ==, "w");
    g_assert_true(test->m_events[3].isComposing);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(w), 3);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_true(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionUpdate);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "ww");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[2].keyCode, ==, 87);
    g_assert_cmpstr(test->m_events[2].key.data(), ==, "w");
    g_assert_true(test->m_events[2].isComposing);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(space), 3);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_true(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionEnd);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "w");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[2].keyCode, ==, 32);
    g_assert_cmpstr(test->m_events[2].key.data(), ==, " ");
    g_assert_false(test->m_events[2].isComposing);
    {
        auto editableValue = test->editableValue();
        g_assert_cmpstr(editableValue.get(), ==, "w");
    }
    test->resetEditable();
}

static void testWebKitInputMethodContextCancelSequence(InputMethodTest* test, gconstpointer)
{
    test->loadHtml(testHTML, nullptr);
    test->waitUntilLoadFinished();

    test->focusEditableAndWaitUntilInputMethodEnabled();

    // Compose w + w + Escape -> cancel sequence.
    test->keyStrokeAndWaitForEvents(KEY(w), 4, CONTROL_MASK | SHIFT_MASK);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_false(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionStart);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::CompositionUpdate);
    g_assert_cmpstr(test->m_events[2].data.data(), ==, "w");
    g_assert_true(test->m_events[3].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[3].keyCode, ==, 87);
    g_assert_cmpstr(test->m_events[3].key.data(), ==, "w");
    g_assert_true(test->m_events[3].isComposing);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(Escape), 3);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_true(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionEnd);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[2].keyCode, ==, 27);
    g_assert_cmpstr(test->m_events[2].key.data(), ==, "Escape");
    g_assert_false(test->m_events[2].isComposing);
    {
        auto editableValue = test->editableValue();
        g_assert_cmpstr(editableValue.get(), ==, "");
    }
    test->resetEditable();
}

static void testWebKitInputMethodContextSurrounding(InputMethodTest* test, gconstpointer)
{
    test->loadHtml(testHTML, nullptr);
    test->waitUntilLoadFinished();

    test->focusEditableAndWaitUntilInputMethodEnabled();

    g_assert_null(test->surroundingText());
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 0);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());

    test->keyStrokeAndWaitForEvents(KEY(a), 3);
    test->keyStrokeAndWaitForEvents(KEY(b), 6);
    test->keyStrokeAndWaitForEvents(KEY(c), 9);
    g_assert_cmpstr(test->surroundingText(), ==, "abc");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 3);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->m_events.clear();

    // Check preedit string is not included in surrounding.
    // 1. Preedit string at the beginning of context.
    test->keyStrokeAndWaitForEvents(KEY(Left), 2);
    test->keyStrokeAndWaitForEvents(KEY(Left), 4);
    test->keyStrokeAndWaitForEvents(KEY(Left), 6);
    g_assert_cmpstr(test->surroundingText(), ==, "abc");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 0);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(w), 4, CONTROL_MASK | SHIFT_MASK);
    g_assert_cmpstr(test->surroundingText(), ==, "abc");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 0);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->keyStrokeAndWaitForEvents(KEY(g), 7);
    test->keyStrokeAndWaitForEvents(KEY(t), 10);
    test->keyStrokeAndWaitForEvents(KEY(k), 13);
    g_assert_cmpstr(test->surroundingText(), ==, "abc");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 0);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->keyStrokeAndWaitForEvents(KEY(ISO_Enter), 16);
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKabc");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 9);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->m_events.clear();
    // 2. Preedit string in the middle of context.
    test->keyStrokeAndWaitForEvents(KEY(w), 4, CONTROL_MASK | SHIFT_MASK);
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKabc");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 9);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->keyStrokeAndWaitForEvents(KEY(w), 7);
    test->keyStrokeAndWaitForEvents(KEY(p), 10);
    test->keyStrokeAndWaitForEvents(KEY(e), 13);
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKabc");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 9);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->keyStrokeAndWaitForEvents(KEY(space), 16);
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKWPEWebKitabc");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 18);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->m_events.clear();
    // 3. Preedit string at the end of context.
    test->keyStrokeAndWaitForEvents(KEY(Right), 2);
    test->keyStrokeAndWaitForEvents(KEY(Right), 4);
    test->keyStrokeAndWaitForEvents(KEY(Right), 6);
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKWPEWebKitabc");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 21);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(w), 4, CONTROL_MASK | SHIFT_MASK);
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKWPEWebKitabc");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 21);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->keyStrokeAndWaitForEvents(KEY(g), 7);
    test->keyStrokeAndWaitForEvents(KEY(t), 10);
    test->keyStrokeAndWaitForEvents(KEY(k), 13);
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKWPEWebKitabc");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 21);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->keyStrokeAndWaitForEvents(KEY(ISO_Enter), 16);
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKWPEWebKitabcWebKitGTK");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 30);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->m_events.clear();

    // Check selection cursor.
    test->keyStrokeAndWaitForEvents(KEY(Left), 2, SHIFT_MASK);
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKWPEWebKitabcWebKitGTK");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 29);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, 30);
    test->keyStrokeAndWaitForEvents(KEY(Home), 4, SHIFT_MASK);
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 0);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, 30);
    test->keyStrokeAndWaitForEvents(KEY(Left), 6);
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 0);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(Right), 2, SHIFT_MASK);
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKWPEWebKitabcWebKitGTK");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 0);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, 1);
    test->keyStrokeAndWaitForEvents(KEY(End), 4, SHIFT_MASK);
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 0);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, 30);
    test->keyStrokeAndWaitForEvents(KEY(Right), 6);
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKWPEWebKitabcWebKitGTK");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 30);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->m_events.clear();

    // Check text replacements (get surrounding + delete surrounding).
    test->keyStrokeAndWaitForEvents(KEY(colon), 3);
    test->keyStrokeAndWaitForEvents(KEY(minus), 6);
    test->keyStrokeAndWaitForEvents(KEY(parenright), 9);
    test->waitForSurroundingText("WebKitGTKWPEWebKitabcWebKitGTK😀️");
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKWPEWebKitabcWebKitGTK😀️");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 37);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->m_events.clear();

    // Check multiline context.
    test->keyStrokeAndWaitForEvents(KEY(Return), 3);
    test->keyStrokeAndWaitForEvents(KEY(a), 6);
    test->waitForSurroundingText("WebKitGTKWPEWebKitabcWebKitGTK😀️\na");
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKWPEWebKitabcWebKitGTK😀️\na");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 39);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->m_events.clear();
}

static void testWebKitInputMethodContextReset(InputMethodTest* test, gconstpointer)
{
    test->loadHtml(testHTML, nullptr);
    test->waitUntilLoadFinished();

    test->focusEditableAndWaitUntilInputMethodEnabled();

    // Compose w + w + click -> reset sequence.
    test->keyStrokeAndWaitForEvents(KEY(w), 4, CONTROL_MASK | SHIFT_MASK);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::KeyDown);
    g_assert_cmpuint(test->m_events[0].keyCode, ==, 229);
    g_assert_cmpstr(test->m_events[0].key.data(), ==, "Unidentified");
    g_assert_false(test->m_events[0].isComposing);
    g_assert_true(test->m_events[1].type == InputMethodTest::Event::Type::CompositionStart);
    g_assert_cmpstr(test->m_events[1].data.data(), ==, "");
    g_assert_true(test->m_events[2].type == InputMethodTest::Event::Type::CompositionUpdate);
    g_assert_cmpstr(test->m_events[2].data.data(), ==, "w");
    g_assert_true(test->m_events[3].type == InputMethodTest::Event::Type::KeyUp);
    g_assert_cmpuint(test->m_events[3].keyCode, ==, 87);
    g_assert_cmpstr(test->m_events[3].key.data(), ==, "w");
    g_assert_true(test->m_events[3].isComposing);
    test->m_events.clear();
    test->clickAndWaitForEvents(1);
    g_assert_true(test->m_events[0].type == InputMethodTest::Event::Type::CompositionEnd);
    g_assert_cmpstr(test->m_events[0].data.data(), ==, "w");
    {
        auto editableValue = test->editableValue();
        g_assert_cmpstr(editableValue.get(), ==, "w");
    }
    test->resetEditable();
}

static void testWebKitInputMethodContextContentType(InputMethodTest* test, gconstpointer)
{
    test->loadHtml("<input id='editable' spellcheck='false'></input>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='number' spellcheck='false'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_NUMBER);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='number' spellcheck='false' pattern='[0-9]*'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_DIGITS);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='text' spellcheck='false' pattern='\\d*'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_DIGITS);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='tel' spellcheck='false'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_PHONE);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='url' spellcheck='false'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_URL);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='email' spellcheck='false'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_EMAIL);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='password' spellcheck='false'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_PASSWORD);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='text' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='decimal' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_NUMBER);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='numeric' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_DIGITS);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='tel' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_PHONE);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='email' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_EMAIL);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='url' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_URL);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='search' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_NONE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='none' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<textarea id='editable'></textarea>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_SPELLCHECK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<textarea id='editable' autocapitalize='none'></textarea>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_SPELLCHECK | WEBKIT_INPUT_HINT_LOWERCASE);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<textarea id='editable' autocapitalize='sentences'></textarea>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_SPELLCHECK | WEBKIT_INPUT_HINT_UPPERCASE_SENTENCES);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<textarea id='editable' autocapitalize='words'></textarea>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_SPELLCHECK | WEBKIT_INPUT_HINT_UPPERCASE_WORDS);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<textarea id='editable' autocapitalize='characters'></textarea>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_SPELLCHECK | WEBKIT_INPUT_HINT_UPPERCASE_CHARS);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();
}

void beforeAll()
{
    InputMethodTest::add("WebKitInputMethodContext", "simple", testWebKitInputMethodContextSimple);
    InputMethodTest::add("WebKitInputMethodContext", "sequence", testWebKitInputMethodContextSequence);
    InputMethodTest::add("WebKitInputMethodContext", "invalid-sequence", testWebKitInputMethodContextInvalidSequence);
    InputMethodTest::add("WebKitInputMethodContext", "cancel-sequence", testWebKitInputMethodContextCancelSequence);
    InputMethodTest::add("WebKitInputMethodContext", "surrounding", testWebKitInputMethodContextSurrounding);
    InputMethodTest::add("WebKitInputMethodContext", "reset", testWebKitInputMethodContextReset);
    InputMethodTest::add("WebKitInputMethodContext", "content-type", testWebKitInputMethodContextContentType);
}

void afterAll()
{

}
