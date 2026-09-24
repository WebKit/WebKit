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

#include "BrowserLoggingInputMethodContext.h"

struct _BrowserLoggingInputMethodContext {
    WebKitInputMethodContext parent;

    gboolean focused;
};

G_DEFINE_TYPE(BrowserLoggingInputMethodContext, browser_logging_input_method_context, WEBKIT_TYPE_INPUT_METHOD_CONTEXT)

static const char *purposeName(WebKitInputPurpose purpose)
{
    switch (purpose) {
    case WEBKIT_INPUT_PURPOSE_FREE_FORM:
        return "FreeForm";
    case WEBKIT_INPUT_PURPOSE_DIGITS:
        return "Digits";
    case WEBKIT_INPUT_PURPOSE_NUMBER:
        return "Number";
    case WEBKIT_INPUT_PURPOSE_PHONE:
        return "Phone";
    case WEBKIT_INPUT_PURPOSE_URL:
        return "Url";
    case WEBKIT_INPUT_PURPOSE_EMAIL:
        return "Email";
    case WEBKIT_INPUT_PURPOSE_PASSWORD:
        return "Password";
    case WEBKIT_INPUT_PURPOSE_SEARCH:
        return "Search";
    }

    return "Unknown";
}

static char *hintsDescription(WebKitInputHints hints)
{
    GString *description;

    if (!hints)
        return g_strdup("None");

    description = g_string_new(NULL);
#define APPEND_HINT(hint, name) \
    if (hints & hint) \
        g_string_append(description, description->len ? "|" name : name);
    APPEND_HINT(WEBKIT_INPUT_HINT_SPELLCHECK, "Spellcheck");
    APPEND_HINT(WEBKIT_INPUT_HINT_LOWERCASE, "Lowercase");
    APPEND_HINT(WEBKIT_INPUT_HINT_UPPERCASE_CHARS, "UppercaseChars");
    APPEND_HINT(WEBKIT_INPUT_HINT_UPPERCASE_WORDS, "UppercaseWords");
    APPEND_HINT(WEBKIT_INPUT_HINT_UPPERCASE_SENTENCES, "UppercaseSentences");
    APPEND_HINT(WEBKIT_INPUT_HINT_INHIBIT_OSK, "InhibitOSK");
#undef APPEND_HINT

    return g_string_free(description, FALSE);
}

static void logContentType(WebKitInputMethodContext *context, const char *event)
{
    g_autofree char *hints = hintsDescription(webkit_input_method_context_get_input_hints(context));
    g_printerr("input-method: %s: purpose=%s hints=%s\n", event, purposeName(webkit_input_method_context_get_input_purpose(context)), hints);
}

static void browserLoggingInputMethodContextDispatchPropertiesChanged(GObject *object, guint propertyCount, GParamSpec **properties)
{
    guint i;

    G_OBJECT_CLASS(browser_logging_input_method_context_parent_class)->dispatch_properties_changed(object, propertyCount, properties);

    if (!BROWSER_LOGGING_INPUT_METHOD_CONTEXT(object)->focused)
        return;

    for (i = 0; i < propertyCount; i++) {
        if (g_str_equal(properties[i]->name, "input-purpose") || g_str_equal(properties[i]->name, "input-hints")) {
            logContentType(WEBKIT_INPUT_METHOD_CONTEXT(object), "content type");
            return;
        }
    }
}

static void browserLoggingInputMethodContextSetEnablePreedit(WebKitInputMethodContext *context G_GNUC_UNUSED, gboolean enabled)
{
    g_printerr("input-method: set enable preedit: %s\n", enabled ? "yes" : "no");
}

static void browserLoggingInputMethodContextNotifyFocusIn(WebKitInputMethodContext *context)
{
    BROWSER_LOGGING_INPUT_METHOD_CONTEXT(context)->focused = TRUE;
    logContentType(context, "focus in");
}

static void browserLoggingInputMethodContextNotifyFocusOut(WebKitInputMethodContext *context)
{
    BROWSER_LOGGING_INPUT_METHOD_CONTEXT(context)->focused = FALSE;
    g_printerr("input-method: focus out\n");
}

static void browserLoggingInputMethodContextNotifyCursorArea(WebKitInputMethodContext *context G_GNUC_UNUSED, int x, int y, int width, int height)
{
    g_printerr("input-method: cursor area: %d,%d %dx%d\n", x, y, width, height);
}

static void browserLoggingInputMethodContextNotifySurrounding(WebKitInputMethodContext *context G_GNUC_UNUSED, const char *text G_GNUC_UNUSED, guint length, guint cursorIndex, guint selectionIndex)
{
    g_printerr("input-method: surrounding: %u bytes, cursor=%u, selection=%u\n", length, cursorIndex, selectionIndex);
}

static void browserLoggingInputMethodContextReset(WebKitInputMethodContext *context G_GNUC_UNUSED)
{
    g_printerr("input-method: reset\n");
}

static void browser_logging_input_method_context_class_init(BrowserLoggingInputMethodContextClass *klass)
{
    GObjectClass *objectClass = G_OBJECT_CLASS(klass);
    objectClass->dispatch_properties_changed = browserLoggingInputMethodContextDispatchPropertiesChanged;

    WebKitInputMethodContextClass *contextClass = WEBKIT_INPUT_METHOD_CONTEXT_CLASS(klass);
    contextClass->set_enable_preedit = browserLoggingInputMethodContextSetEnablePreedit;
    contextClass->notify_focus_in = browserLoggingInputMethodContextNotifyFocusIn;
    contextClass->notify_focus_out = browserLoggingInputMethodContextNotifyFocusOut;
    contextClass->notify_cursor_area = browserLoggingInputMethodContextNotifyCursorArea;
    contextClass->notify_surrounding = browserLoggingInputMethodContextNotifySurrounding;
    contextClass->reset = browserLoggingInputMethodContextReset;
}

static void browser_logging_input_method_context_init(BrowserLoggingInputMethodContext *context G_GNUC_UNUSED)
{
}

WebKitInputMethodContext *browser_logging_input_method_context_new(void)
{
    return WEBKIT_INPUT_METHOD_CONTEXT(g_object_new(BROWSER_TYPE_LOGGING_INPUT_METHOD_CONTEXT, NULL));
}
