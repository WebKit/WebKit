/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "WebViewTest.h"
#include <algorithm>
#include <wtf/glib/GUniquePtr.h>

#if PLATFORM(GTK)
#include <WebKit/GtkVersioning.h>
#if USE(GTK4)
#include <webkit/WebKitWebViewBaseInternal.h>
using PlatformEventKey = GdkEvent;
#else
using PlatformEventKey = GdkEventKey;
#endif
#elif PLATFORM(WPE)
using PlatformEventKey = void;
#endif

struct MockCursorArea {
    int x;
    int y;
    int width;
    int height;
};

typedef struct _WebKitInputMethodContextMock {
    WebKitInputMethodContext parent;

    bool enabled;
    GString* preedit;
    bool commitNextCharacter;
    // Offset of the input method caret inside the preedit, or -1 for the end of the preedit.
    // Counted in bytes, like GString::len, which matches the ASCII preedits used here.
    int preeditCursorOffset;
    char* surroundingText;
    unsigned surroundingCursorIndex;
    unsigned surroundingSelectionIndex;
    MockCursorArea cursorArea;
    unsigned focusInCount;
    unsigned focusOutCount;
    unsigned cursorAreaCount;
    unsigned surroundingCount;
#if ENABLE(WPE_PLATFORM)
    bool usingWPEPlatformAPI;
#endif
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
    if (cursorOffset) {
        if (!mock->preedit)
            *cursorOffset = 0;
        else if (mock->preeditCursorOffset < 0)
            *cursorOffset = mock->preedit->len;
        else
            *cursorOffset = std::min<unsigned>(mock->preeditCursorOffset, mock->preedit->len);
    }
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
    bool isControl = state & GDK_CONTROL_MASK;
    bool isShift = state & GDK_SHIFT_MASK;
#elif PLATFORM(WPE)
    uint32_t keyval = 0;
    bool isKeyPress = false;
    gunichar character = 0;
    bool isControl = false;
    bool isShift = false;
#if ENABLE(WPE_PLATFORM)
    if (mock->usingWPEPlatformAPI) {
        auto* wpeKeyEvent = static_cast<WPEEvent*>(keyEvent);
        keyval = wpe_event_keyboard_get_keyval(wpeKeyEvent);
        isKeyPress = wpe_event_get_event_type(wpeKeyEvent) == WPE_EVENT_KEYBOARD_KEY_DOWN;
        character = wpe_keyval_to_unicode(keyval);
        auto state = wpe_event_get_modifiers(wpeKeyEvent);
        isControl = state & WPE_MODIFIER_KEYBOARD_CONTROL;
        isShift = state & WPE_MODIFIER_KEYBOARD_SHIFT;
    } else
#endif
    {
#if USE(LIBWPE)
        struct wpe_input_keyboard_event* wpeKeyEvent = static_cast<struct wpe_input_keyboard_event*>(keyEvent);
        keyval = wpeKeyEvent->key_code;
        isKeyPress = wpeKeyEvent->pressed;
        character = wpe_key_code_to_unicode(keyval);
        isControl = wpeKeyEvent->modifiers & wpe_input_keyboard_modifier_control;
        isShift = wpeKeyEvent->modifiers & wpe_input_keyboard_modifier_shift;
#endif
    }
#endif
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
    auto* mock = reinterpret_cast<WebKitInputMethodContextMock*>(context);
    mock->enabled = true;
    mock->focusInCount++;
}

static void webkitInputMethodContextMockNotifyFocusOut(WebKitInputMethodContext* context)
{
    auto* mock = reinterpret_cast<WebKitInputMethodContextMock*>(context);
    mock->enabled = false;
    mock->focusOutCount++;
}

static void webkitInputMethodContextMockNotifyCursorArea(WebKitInputMethodContext* context, int x, int y, int width, int height)
{
    auto* mock = reinterpret_cast<WebKitInputMethodContextMock*>(context);
    mock->cursorArea = { x, y, width, height };
    mock->cursorAreaCount++;
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
    mock->surroundingCount++;
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

#if ENABLE(WPE_PLATFORM)
static void webkitInputMethodContextMockSetInUsingWPEPlatformAPI(WebKitInputMethodContextMock* mock)
{
    mock->usingWPEPlatformAPI = true;
}
#endif

static void webkit_input_method_context_mock_class_init(WebKitInputMethodContextMockClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->finalize = webkitInputMethodContextMockFinalize;

    auto* imClass = WEBKIT_INPUT_METHOD_CONTEXT_CLASS(klass);
    imClass->get_preedit = webkitInputMethodContextMockGetPreedit;
    imClass->filter_key_event = webkitInputMethodContextMockFilterKeyEvent;
    imClass->notify_focus_in = webkitInputMethodContextMockNotifyFocusIn;
    imClass->notify_focus_out = webkitInputMethodContextMockNotifyFocusOut;
    imClass->notify_cursor_area = webkitInputMethodContextMockNotifyCursorArea;
    imClass->notify_surrounding = webkitInputMethodContextMockNotifySurrounding;
    imClass->reset = webkitInputMethodContextMockReset;
}

static void webkit_input_method_context_mock_init(WebKitInputMethodContextMock* mock)
{
    mock->preeditCursorOffset = -1;
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

#if ENABLE(2022_GLIB_API)
    static void imEventCallback(WebKitUserContentManager*, JSCValue* result, InputMethodTest* test)
    {
        test->imEvent(result);
    }
#else
    static void imEventCallback(WebKitUserContentManager*, WebKitJavascriptResult* result, InputMethodTest* test)
    {
        test->imEvent(webkit_javascript_result_get_js_value(result));
    }
#endif

    InputMethodTest()
        : m_context(adoptGRef(static_cast<WebKitInputMethodContextMock*>(g_object_new(webkit_input_method_context_mock_get_type(), nullptr))))
    {
#if ENABLE(WPE_PLATFORM)
        if (m_display)
            webkitInputMethodContextMockSetInUsingWPEPlatformAPI(m_context.get());
#endif
        WebViewTest::showInWindow();
#if PLATFORM(GTK)
        auto* defaultContext = webkit_web_view_get_input_method_context(m_webView.get());
        g_assert_true(WEBKIT_IS_INPUT_METHOD_CONTEXT(defaultContext));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(defaultContext));
#elif PLATFORM(WPE)
#if ENABLE(WPE_PLATFORM)
        if (m_display) {
            auto* defaultContext = webkit_web_view_get_input_method_context(m_webView.get());
            g_assert_true(WEBKIT_IS_INPUT_METHOD_CONTEXT(defaultContext));
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(defaultContext));
        } else
            g_assert_null(webkit_web_view_get_input_method_context(m_webView.get()));
#else
        g_assert_null(webkit_web_view_get_input_method_context(m_webView.get()));
#endif
#endif
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_context.get()));
        webkit_web_view_set_input_method_context(m_webView.get(), WEBKIT_INPUT_METHOD_CONTEXT(m_context.get()));
        g_assert_true(webkit_web_view_get_input_method_context(m_webView.get()) == WEBKIT_INPUT_METHOD_CONTEXT(m_context.get()));

#if !ENABLE(2022_GLIB_API)
        webkit_user_content_manager_register_script_message_handler(m_userContentManager.get(), "imEvent");
#else
        webkit_user_content_manager_register_script_message_handler(m_userContentManager.get(), "imEvent", nullptr);
#endif
        g_signal_connect(m_userContentManager.get(), "script-message-received::imEvent", G_CALLBACK(imEventCallback), this);
        g_signal_connect_swapped(m_context.get(), "notify::input-purpose", G_CALLBACK(contentTypeNotifiedCallback), this);
        g_signal_connect_swapped(m_context.get(), "notify::input-hints", G_CALLBACK(contentTypeNotifiedCallback), this);
    }

    static void contentTypeNotifiedCallback(InputMethodTest* test)
    {
        test->m_contentTypeNotificationCount++;
    }

    ~InputMethodTest()
    {
#if !ENABLE(2022_GLIB_API)
        webkit_user_content_manager_unregister_script_message_handler(m_userContentManager.get(), "imEvent");
#else
        webkit_user_content_manager_unregister_script_message_handler(m_userContentManager.get(), "imEvent", nullptr);
#endif
        g_signal_handlers_disconnect_by_data(m_userContentManager.get(), this);
        g_signal_handlers_disconnect_by_data(m_context.get(), this);
    }

    void imEvent(JSCValue* jsEvent)
    {
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
            m_events.append(WTF::move(event));
        } else if (!g_strcmp0(strValue.get(), "keyPress")) {
            InputMethodTest::Event event(InputMethodTest::Event::Type::KeyPress);
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "keyCode"));
            g_assert_true(jsc_value_is_number(value.get()));
            event.keyCode = jsc_value_to_int32(value.get());
            m_events.append(WTF::move(event));
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
            m_events.append(WTF::move(event));
        } else if (!g_strcmp0(strValue.get(), "compositionstart")) {
            InputMethodTest::Event event(InputMethodTest::Event::Type::CompositionStart);
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "data"));
            g_assert_true(jsc_value_is_string(value.get()));
            strValue.reset(jsc_value_to_string(value.get()));
            event.data = strValue.get();
            m_events.append(WTF::move(event));
        } else if (!g_strcmp0(strValue.get(), "compositionupdate")) {
            InputMethodTest::Event event(InputMethodTest::Event::Type::CompositionUpdate);
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "data"));
            g_assert_true(jsc_value_is_string(value.get()));
            strValue.reset(jsc_value_to_string(value.get()));
            event.data = strValue.get();
            m_events.append(WTF::move(event));
        } else if (!g_strcmp0(strValue.get(), "compositionend")) {
            InputMethodTest::Event event(InputMethodTest::Event::Type::CompositionEnd);
            value = adoptGRef(jsc_value_object_get_property(jsEvent, "data"));
            g_assert_true(jsc_value_is_string(value.get()));
            strValue.reset(jsc_value_to_string(value.get()));
            event.data = strValue.get();
            m_events.append(WTF::move(event));
        }

        if (m_events.size() == m_eventsExpected)
            g_main_loop_quit(m_mainLoop);
    }

    void waitUntilInputMethodEnabled()
    {
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

    void focusEditableAndWaitUntilInputMethodEnabled()
    {
        g_assert_false(m_context->enabled);
        runJavaScriptAndWaitUntilFinished("document.getElementById('editable').focus()", nullptr);
        waitUntilInputMethodEnabled();
    }

    void waitUntilInputMethodDisabled()
    {
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

    void unfocusEditableAndWaitUntilInputMethodDisabled()
    {
        g_assert_true(m_context->enabled);
        runJavaScriptAndWaitUntilFinished("document.getElementById('editable').blur()", nullptr);
        waitUntilInputMethodDisabled();
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

    unsigned editableSelectionStart()
    {
        auto* jsResult = runJavaScriptAndWaitUntilFinished("document.getElementById('editable').selectionStart", nullptr);
        return WebViewTest::javascriptResultToNumber(jsResult);
    }

    void keyStrokeAndWaitForEvents(unsigned keyval, unsigned eventsCount, OptionSet<Modifiers> modifiers = OptionSet<Modifiers>())
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
        clickMouseButton(0, 0);
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

    MockCursorArea cursorArea() const
    {
        return m_context->cursorArea;
    }

    unsigned cursorAreaCount() const
    {
        return m_context->cursorAreaCount;
    }

    unsigned surroundingCount() const
    {
        return m_context->surroundingCount;
    }

    unsigned focusInCount() const
    {
        return m_context->focusInCount;
    }

    unsigned focusOutCount() const
    {
        return m_context->focusOutCount;
    }

    unsigned contentTypeNotificationCount() const
    {
        return m_contentTypeNotificationCount;
    }

    bool isInputMethodEnabled() const
    {
        return m_context->enabled;
    }

    void clearInputMethodCounters()
    {
        m_context->focusInCount = 0;
        m_context->focusOutCount = 0;
        m_context->cursorAreaCount = 0;
        m_context->surroundingCount = 0;
        m_contentTypeNotificationCount = 0;
    }

    void setPreeditCursorOffset(int offset) { m_context->preeditCursorOffset = offset; }

    void waitForCursorAreaCount(unsigned count)
    {
        if (m_context->cursorAreaCount >= count)
            return;

        m_expectedCursorAreaCount = count;
        m_cursorAreaSourceID = g_idle_add([](gpointer userData) -> gboolean {
            auto* test = static_cast<InputMethodTest*>(userData);
            if (test->m_context->cursorAreaCount >= test->m_expectedCursorAreaCount) {
                test->m_cursorAreaSourceID = 0;
                test->quitMainLoop();
                return FALSE;
            }

            return TRUE;
        }, this);
        g_main_loop_run(m_mainLoop);
        g_clear_handle_id(&m_cursorAreaSourceID, g_source_remove);
        m_expectedCursorAreaCount = 0;
        g_assert_cmpuint(m_context->cursorAreaCount, >=, count);
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
    unsigned m_contentTypeNotificationCount { 0 };
    unsigned m_expectedCursorAreaCount { 0 };
    unsigned m_cursorAreaSourceID { 0 };
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
    test->keyStrokeAndWaitForEvents(KEY(w), 4, { WebViewTest::Modifiers::Control, WebViewTest::Modifiers::Shift });
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
    test->keyStrokeAndWaitForEvents(KEY(w), 4, { WebViewTest::Modifiers::Control, WebViewTest::Modifiers::Shift });
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
    test->keyStrokeAndWaitForEvents(KEY(w), 4, { WebViewTest::Modifiers::Control, WebViewTest::Modifiers::Shift });
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
    test->keyStrokeAndWaitForEvents(KEY(w), 4, { WebViewTest::Modifiers::Control, WebViewTest::Modifiers::Shift });
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
    test->keyStrokeAndWaitForEvents(KEY(w), 4, { WebViewTest::Modifiers::Control, WebViewTest::Modifiers::Shift });
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
    test->keyStrokeAndWaitForEvents(KEY(w), 4, { WebViewTest::Modifiers::Control, WebViewTest::Modifiers::Shift });
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
    test->keyStrokeAndWaitForEvents(KEY(w), 4, { WebViewTest::Modifiers::Control, WebViewTest::Modifiers::Shift });
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
    test->keyStrokeAndWaitForEvents(KEY(Left), 2, { WebViewTest::Modifiers::Shift });
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKWPEWebKitabcWebKitGTK");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 29);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, 30);
    test->keyStrokeAndWaitForEvents(KEY(Home), 4, { WebViewTest::Modifiers::Shift });
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 0);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, 30);
    test->keyStrokeAndWaitForEvents(KEY(Left), 6);
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 0);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, test->surroundingCursorIndex());
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(Right), 2, { WebViewTest::Modifiers::Shift });
    g_assert_cmpstr(test->surroundingText(), ==, "WebKitGTKWPEWebKitabcWebKitGTK");
    g_assert_cmpuint(test->surroundingCursorIndex(), ==, 0);
    g_assert_cmpuint(test->surroundingSelectionIndex(), ==, 1);
    test->keyStrokeAndWaitForEvents(KEY(End), 4, { WebViewTest::Modifiers::Shift });
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
    test->keyStrokeAndWaitForEvents(KEY(w), 4, { WebViewTest::Modifiers::Control, WebViewTest::Modifiers::Shift });
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

// Two fields whose InputMethodState compares equal, the case WebPage::setInputMethodState
// short-circuits.
static const char* twoFieldsHTML = "<html><body>"
    "<input id='editable' type='text' value='one' spellcheck='false'>"
    "<input id='second' type='text' value='two' spellcheck='false'>"
    "</body></html>";

// The same two fields, both empty, so that the surrounding text does not change either.
static const char* twoEmptyFieldsHTML = "<html><body>"
    "<input id='editable' type='text' spellcheck='false'>"
    "<input id='second' type='text' spellcheck='false'>"
    "</body></html>";

static void testWebKitInputMethodContextCursorArea(InputMethodTest* test, gconstpointer)
{
    test->loadHtml(testHTML, nullptr);
    test->waitUntilLoadFinished();

    test->focusEditableAndWaitUntilInputMethodEnabled();

    // One character moves the caret by less than the 10px threshold in notifyCursorRect, so type
    // enough of them to be sure a notification is sent whatever the caret did on focus.
    auto areaCountBeforeTyping = test->cursorAreaCount();
    test->keyStrokeAndWaitForEvents(KEY(a), 3);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(b), 3);
    test->m_events.clear();
    test->waitForCursorAreaCount(areaCountBeforeTyping + 1);
    auto firstArea = test->cursorArea();
    g_assert_cmpint(firstArea.width, >, 0);
    g_assert_cmpint(firstArea.height, >, 0);
    test->m_events.clear();

    auto areaCountBeforeMoving = test->cursorAreaCount();
    test->keyStrokeAndWaitForEvents(KEY(c), 3);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(d), 3);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(e), 3);
    test->m_events.clear();
    test->waitForCursorAreaCount(areaCountBeforeMoving + 1);
    g_assert_cmpint(test->cursorArea().x, >, firstArea.x);
}

static void testWebKitInputMethodContextPreeditCursor(InputMethodTest* test, gconstpointer)
{
    test->loadHtml(testHTML, nullptr);
    test->waitUntilLoadFinished();

    test->focusEditableAndWaitUntilInputMethodEnabled();

    // Report the input method caret inside the preedit rather than at its end.
    test->setPreeditCursorOffset(1);

    test->keyStrokeAndWaitForEvents(KEY(w), 4, { WebViewTest::Modifiers::Control, WebViewTest::Modifiers::Shift });
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(g), 3);
    test->m_events.clear();
    test->keyStrokeAndWaitForEvents(KEY(t), 3);
    test->m_events.clear();

    {
        auto editableValue = test->editableValue();
        g_assert_cmpstr(editableValue.get(), ==, "wgt");
    }

    // The composition starts where the input method said its caret was, one code unit in. With the
    // caret left at the end of the preedit this would be 3.
    g_assert_cmpuint(test->editableSelectionStart(), ==, 1);

    test->keyStrokeAndWaitForEvents(KEY(Escape), 3);
}

static void testWebKitInputMethodContextFocusChange(InputMethodTest* test, gconstpointer)
{
    test->loadHtml(twoFieldsHTML, nullptr);
    test->waitUntilLoadFinished();

    test->focusEditableAndWaitUntilInputMethodEnabled();
    test->waitForSurroundingText("one");
    g_assert_cmpstr(test->surroundingText(), ==, "one");

    test->clearInputMethodCounters();
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('second').focus()", nullptr);
    test->waitForSurroundingText("two");

    // The keyboard must stay open across the move: no focus out/in cycle, no content type change.
    g_assert_cmpuint(test->focusOutCount(), ==, 0);
    g_assert_cmpuint(test->focusInCount(), ==, 0);
    g_assert_cmpuint(test->contentTypeNotificationCount(), ==, 0);

    // Here the new surrounding text is the only hint the embedder gets that anything happened.
    g_assert_cmpstr(test->surroundingText(), ==, "two");

    // Repeat the move between two empty fields, where the surrounding text does not change either.
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('second').blur()", nullptr);
    test->waitUntilInputMethodDisabled();
    test->loadHtml(twoEmptyFieldsHTML, nullptr);
    test->waitUntilLoadFinished();

    test->focusEditableAndWaitUntilInputMethodEnabled();
    test->clearInputMethodCounters();
    test->runJavaScriptAndWaitUntilFinished("document.getElementById('second').focus()", nullptr);
    test->assertJavaScriptBecomesTrue("document.activeElement.id === 'second'");

    g_assert_cmpuint(test->focusOutCount(), ==, 0);
    g_assert_cmpuint(test->focusInCount(), ==, 0);
    g_assert_cmpuint(test->contentTypeNotificationCount(), ==, 0);

    // surroundingCount() stays 0: nothing at all reaches the embedder.
    // This is a gap, not intentional behaviour.
}

static void testWebKitInputMethodContextFocusInteraction(InputMethodTest* test, gconstpointer)
{
    test->loadHtml("<input id='editable' type='text' spellcheck='false'>", nullptr);
    test->waitUntilLoadFinished();

    // Focusing by click is a user interaction, so the on screen keyboard is not inhibited. Check
    // the click landed on the field before waiting, so a miss fails instead of hanging.
    g_assert_false(test->isInputMethodEnabled());
    test->clickMouseButton(20, 20);
    test->assertJavaScriptBecomesTrue("document.activeElement.id === 'editable'");
    test->waitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints() & WEBKIT_INPUT_HINT_INHIBIT_OSK, ==, 0);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    // element.focus() is not, which is why the content-type test expects INHIBIT_OSK everywhere.
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->hints() & WEBKIT_INPUT_HINT_INHIBIT_OSK, ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
}

static void testWebKitInputMethodContextContentType(InputMethodTest* test, gconstpointer)
{
    test->loadHtml("<input id='editable' spellcheck='false'></input>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='number' spellcheck='false'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_NUMBER);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='number' spellcheck='false' pattern='[0-9]*'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_DIGITS);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='text' spellcheck='false' pattern='\\d*'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_DIGITS);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='tel' spellcheck='false'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_PHONE);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='url' spellcheck='false'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_URL);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='email' spellcheck='false'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_EMAIL);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='password' spellcheck='false'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_PASSWORD);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='search' spellcheck='false'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_SEARCH);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<input id='editable' type='search' spellcheck='true'>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_SEARCH);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_SPELLCHECK | WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='text' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='decimal' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_NUMBER);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='numeric' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_DIGITS);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='tel' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_PHONE);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='email' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_EMAIL);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='url' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_URL);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<div contenteditable id='editable' inputmode='search' spellcheck='false'></div>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_SEARCH);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_INHIBIT_OSK);
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
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_SPELLCHECK | WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<textarea id='editable' autocapitalize='none'></textarea>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_SPELLCHECK | WEBKIT_INPUT_HINT_LOWERCASE | WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<textarea id='editable' autocapitalize='sentences'></textarea>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_SPELLCHECK | WEBKIT_INPUT_HINT_UPPERCASE_SENTENCES | WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<textarea id='editable' autocapitalize='words'></textarea>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_SPELLCHECK | WEBKIT_INPUT_HINT_UPPERCASE_WORDS | WEBKIT_INPUT_HINT_INHIBIT_OSK);
    test->unfocusEditableAndWaitUntilInputMethodDisabled();

    test->loadHtml("<textarea id='editable' autocapitalize='characters'></textarea>", nullptr);
    test->waitUntilLoadFinished();
    test->focusEditableAndWaitUntilInputMethodEnabled();
    g_assert_cmpuint(test->purpose(), ==, WEBKIT_INPUT_PURPOSE_FREE_FORM);
    g_assert_cmpuint(test->hints(), ==, WEBKIT_INPUT_HINT_SPELLCHECK | WEBKIT_INPUT_HINT_UPPERCASE_CHARS | WEBKIT_INPUT_HINT_INHIBIT_OSK);
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
    InputMethodTest::add("WebKitInputMethodContext", "cursor-area", testWebKitInputMethodContextCursorArea);
    InputMethodTest::add("WebKitInputMethodContext", "preedit-cursor", testWebKitInputMethodContextPreeditCursor);
    InputMethodTest::add("WebKitInputMethodContext", "focus-change", testWebKitInputMethodContextFocusChange);
    InputMethodTest::add("WebKitInputMethodContext", "focus-interaction", testWebKitInputMethodContextFocusInteraction);
    InputMethodTest::add("WebKitInputMethodContext", "content-type", testWebKitInputMethodContextContentType);
}

void afterAll()
{

}
