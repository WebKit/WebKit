/*
 * Copyright (C) 2015 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
#include "WebKitEditorState.h"

#include "EditorState.h"
#include "WebKitEditorStatePrivate.h"
#include <glib/gi18n-lib.h>
#include <wtf/glib/WTFGType.h>

using namespace WebKit;

/**
 * SECTION: WebKitEditorState
 * @Short_description: Web editor state
 * @Title: WebKitEditorState
 * @See_also: #WebKitWebView
 *
 * WebKitEditorState represents the state of a #WebKitWebView editor.
 * Use webkit_web_view_get_editor_state() to get WebKitEditorState of
 * a #WebKitWebView.
 *
 * Since: 2.10
 */

enum {
    PROP_0,

    PROP_TYPING_ATTRIBUTES
};

struct _WebKitEditorStatePrivate {
    unsigned typingAttributes;
};

WEBKIT_DEFINE_TYPE(WebKitEditorState, webkit_editor_state, G_TYPE_OBJECT)

static void webkitEditorStateGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitEditorState* editorState = WEBKIT_EDITOR_STATE(object);

    switch (propId) {
    case PROP_TYPING_ATTRIBUTES:
        g_value_set_uint(value, webkit_editor_state_get_typing_attributes(editorState));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_editor_state_class_init(WebKitEditorStateClass* editorStateClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(editorStateClass);
    objectClass->get_property = webkitEditorStateGetProperty;

    /**
     * WebKitEditorState:typing-attributes:
     *
     * Bitmask of #WebKitEditorTypingAttributes flags.
     * See webkit_editor_state_get_typing_attributes() for more information.
     *
     * Since: 2.10
     */
    g_object_class_install_property(
        objectClass,
        PROP_TYPING_ATTRIBUTES,
        g_param_spec_uint(
            "typing-attributes",
            _("Typing Attributes"),
            _("Flags with the typing attributes"),
            0, G_MAXUINT, 0,
            WEBKIT_PARAM_READABLE));
}

static void webkitEditorStateSetTypingAttributes(WebKitEditorState* editorState, unsigned typingAttributes)
{
    if (typingAttributes == editorState->priv->typingAttributes)
        return;

    editorState->priv->typingAttributes = typingAttributes;
    g_object_notify(G_OBJECT(editorState), "typing-attributes");
}

WebKitEditorState* webkitEditorStateCreate(const EditorState& state)
{
    WebKitEditorState* editorState = WEBKIT_EDITOR_STATE(g_object_new(WEBKIT_TYPE_EDITOR_STATE, nullptr));
    webkitEditorStateChanged(editorState, state);
    return editorState;
}

void webkitEditorStateChanged(WebKitEditorState* editorState, const EditorState& newState)
{
    if (newState.isMissingPostLayoutData)
        return;

    unsigned typingAttributes = WEBKIT_EDITOR_TYPING_ATTRIBUTE_NONE;
#if PLATFORM(GTK)
    const auto& postLayoutData = newState.postLayoutData();
    if (postLayoutData.typingAttributes & AttributeBold)
        typingAttributes |= WEBKIT_EDITOR_TYPING_ATTRIBUTE_BOLD;
    if (postLayoutData.typingAttributes & AttributeItalics)
        typingAttributes |= WEBKIT_EDITOR_TYPING_ATTRIBUTE_ITALIC;
    if (postLayoutData.typingAttributes & AttributeUnderline)
        typingAttributes |= WEBKIT_EDITOR_TYPING_ATTRIBUTE_UNDERLINE;
    if (postLayoutData.typingAttributes & AttributeStrikeThrough)
        typingAttributes |= WEBKIT_EDITOR_TYPING_ATTRIBUTE_STRIKETHROUGH;
#endif
    webkitEditorStateSetTypingAttributes(editorState, typingAttributes);
}

/**
 * webkit_editor_state_get_typing_attributes:
 * @editor_state: a #WebKitEditorState
 *
 * Gets the typing attributes at the current cursor position.
 * If there is a selection, this returns the typing attributes
 * of the selected text. Note that in case of a selection,
 * typing attributes are considered active only when they are
 * present throughout the selection.
 *
 * Returns: a bitmask of #WebKitEditorTypingAttributes flags
 *
 * Since: 2.10
 */
guint webkit_editor_state_get_typing_attributes(WebKitEditorState* editorState)
{
    g_return_val_if_fail(WEBKIT_IS_EDITOR_STATE(editorState), WEBKIT_EDITOR_TYPING_ATTRIBUTE_NONE);

    return editorState->priv->typingAttributes;
}

