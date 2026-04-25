/*
 * Copyright (C) 2025 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEClipboard.h"

#include "GRefPtrWPE.h"
#include "WPEDisplay.h"
#include <optional>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GWeakPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

/**
 * WPEClipboard:
 *
 */
struct _WPEClipboardPrivate {
    GWeakPtr<WPEDisplay> display;
    int64_t changeCount;
    bool isLocal;
    GRefPtr<WPEClipboardContent> content;
    GRefPtr<GPtrArray> formats;
};
WEBKIT_DEFINE_TYPE(WPEClipboard, wpe_clipboard, G_TYPE_OBJECT)

struct _WPEClipboardContent {
    std::optional<CString> text;
    std::optional<HashMap<const char*, GRefPtr<GBytes>>> buffers;
    int referenceCount { 1 };
};
G_DEFINE_BOXED_TYPE(WPEClipboardContent, wpe_clipboard_content, wpe_clipboard_content_ref, wpe_clipboard_content_unref)

enum {
    PROP_0,

    PROP_DISPLAY,
    PROP_CHANGE_COUNT,

    N_PROPERTIES
};

static std::array<GParamSpec*, N_PROPERTIES> sObjProperties;

static void wpeClipboardSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* clipboard = WPE_CLIPBOARD(object);

    switch (propId) {
    case PROP_DISPLAY:
        clipboard->priv->display.reset(WPE_DISPLAY(g_value_get_object(value)));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeClipboardGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    auto* clipboard = WPE_CLIPBOARD(object);

    switch (propId) {
    case PROP_DISPLAY:
        g_value_set_object(value, wpe_clipboard_get_display(clipboard));
        break;
    case PROP_CHANGE_COUNT:
        g_value_set_int64(value, wpe_clipboard_get_change_count(clipboard));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeClipboardChanged(WPEClipboard* clipboard, GPtrArray* formats, gboolean isLocal, WPEClipboardContent* content)
{
    auto* priv = clipboard->priv;
    priv->isLocal = isLocal;
    priv->formats = formats;
    priv->content = content;

    priv->changeCount++;
    g_object_notify_by_pspec(G_OBJECT(clipboard), sObjProperties[PROP_CHANGE_COUNT]);
}

static void wpe_clipboard_class_init(WPEClipboardClass* clipboardClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(clipboardClass);
    objectClass->set_property = wpeClipboardSetProperty;
    objectClass->get_property = wpeClipboardGetProperty;

    clipboardClass->changed = wpeClipboardChanged;

    /**
     * WPEClipboard:display:
     *
     * The #WPEDisplay of the clipboard.
     */
    sObjProperties[PROP_DISPLAY] =
        g_param_spec_object(
            "display",
            nullptr, nullptr,
            WPE_TYPE_DISPLAY,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WPEClipboard:change-count:
     *
     * A counter of changes in the clipboard.
     */
    sObjProperties[PROP_CHANGE_COUNT] =
        g_param_spec_int64(
            "change-count",
            nullptr, nullptr,
            0, G_MAXINT64, 0,
            WEBKIT_PARAM_READABLE);

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties.data());
}

/**
 * wpe_clipboard_new:
 * @display: a #WPEDisplay
 *
 * Create a new #WPEClipboard for @display. The clipboard created is always local, so its
 * contents are not shared with other applications.
 *
 * Returns: (transfer full): a new #WPEClipboard
 */
WPEClipboard* wpe_clipboard_new(WPEDisplay* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY(display), nullptr);

    return WPE_CLIPBOARD(g_object_new(WPE_TYPE_CLIPBOARD, "display", display, nullptr));
}

/**
 * wpe_clipboard_get_display:
 * @clipboard: a #WPEClipoard
 *
 * Get the #WPEDisplay of @clipboard
 *
 * Returns: (transfer none): a #WPEDisplay
 */
WPEDisplay* wpe_clipboard_get_display(WPEClipboard* clipboard)
{
    g_return_val_if_fail(WPE_IS_CLIPBOARD(clipboard), nullptr);

    return clipboard->priv->display.get();
}

/**
 * wpe_clipboard_get_change_count:
 * @clipboard: a #WPEClipoard
 *
 * Get the amount of times @clipboard changed since it was created.
 *
 * Returns: the change count
 */
gint64 wpe_clipboard_get_change_count(WPEClipboard* clipboard)
{
    g_return_val_if_fail(WPE_IS_CLIPBOARD(clipboard), -1);

    return clipboard->priv->changeCount;
}

/**
 * wpe_clipboard_get_formats:
 * @clipboard: a #WPEClipoard
 *
 * Get the MIME type formats that clipboard can provide
 *
 * Returns: (array zero-terminated=1) (element-type utf8) (transfer none): A %NULL-terminated
 *    array of MIME type formats, or %NULL if clipboard is empty.
 */
const char* const* wpe_clipboard_get_formats(WPEClipboard* clipboard)
{
    g_return_val_if_fail(WPE_IS_CLIPBOARD(clipboard), nullptr);

    auto* priv = clipboard->priv;
    return priv->formats ? reinterpret_cast<char**>(priv->formats->pdata) : nullptr;
}

/**
 * wpe_clipboard_set_content:
 * @clipboard: a #WPEClipoard
 * @content: (transfer none) (nullable): a #WPEClipboardContent, or %NULL to clear the clipboard
 *
 * Set @content on @clipboard. Passing a %NULL @content will clear the @clipboard.
 *
 * If the @clipboard is not local-only the contents will be advertised to the system
 * clipboard.
 */
void wpe_clipboard_set_content(WPEClipboard* clipboard, WPEClipboardContent* content)
{
    g_return_if_fail(WPE_IS_CLIPBOARD(clipboard));

    GRefPtr<GPtrArray> formats;
    if (content) {
        formats = adoptGRef(g_ptr_array_new());
        if (content->text) {
            g_ptr_array_add(formats.get(), static_cast<gpointer>(const_cast<char*>(g_intern_static_string("text/plain"))));
            g_ptr_array_add(formats.get(), static_cast<gpointer>(const_cast<char*>(g_intern_static_string("text/plain;charset=utf-8"))));
        }
        if (content->buffers) {
            for (const auto* format : content->buffers->keys())
                g_ptr_array_add(formats.get(), static_cast<gpointer>(const_cast<char*>(format)));
        }
        g_ptr_array_add(formats.get(), nullptr);
    }

    WPE_CLIPBOARD_GET_CLASS(clipboard)->changed(clipboard, formats.get(), TRUE, content);
}

/**
 * wpe_clipboard_get_content:
 * @clipboard: a #WPEClipoard
 *
 * Get the #WPEClipboardContent previously set with wpe_clipboard_set_content().
 * This function returns %NULL if @clipboard is empty or its contents are owned
 * by the system clipboard.
 *
 * Returns: (transfer none) (nullable): a #WPEClipboardContent or %NULL
 */
WPEClipboardContent* wpe_clipboard_get_content(WPEClipboard* clipboard)
{
    g_return_val_if_fail(WPE_IS_CLIPBOARD(clipboard), nullptr);

    return clipboard->priv->content.get();
}

/**
 * wpe_clipboard_read_bytes:
 * @clipboard: a #WPEClipoard
 * @format: the MIME type format of the text to read
 *
 * Get the contents of @clipboard for the given MIME type @format as bytes.
 *
 * Returns: (transfer full) (nullable): a new #GBytes, or %NULL
 */
GBytes* wpe_clipboard_read_bytes(WPEClipboard* clipboard, const char* format)
{
    g_return_val_if_fail(WPE_IS_CLIPBOARD(clipboard), nullptr);
    g_return_val_if_fail(format && *format, nullptr);

    auto* priv = clipboard->priv;
    const auto* internalFormat = g_intern_string(format);
    if (!priv->formats || !g_ptr_array_find(priv->formats.get(), internalFormat, nullptr))
        return nullptr;

    if (priv->isLocal) {
        if (!priv->content)
            return nullptr;

        GRefPtr<GOutputStream> stream = adoptGRef(g_memory_output_stream_new_resizable());
        if (!wpe_clipboard_content_serialize(priv->content.get(), format, stream.get()))
            return nullptr;

        g_output_stream_close(stream.get(), nullptr, nullptr);
        return g_memory_output_stream_steal_as_bytes(G_MEMORY_OUTPUT_STREAM(stream.get()));
    }

    auto* clipboardClass = WPE_CLIPBOARD_GET_CLASS(clipboard);
    if (clipboardClass->read)
        return clipboardClass->read(clipboard, format);

    return nullptr;
}

/**
 * wpe_clipboard_read_text:
 * @clipboard: a #WPEClipoard
 * @format: the MIME type format of the text to read
 * @size: (out) (optional): location to return size of returned text.
 *
 * Get the contents of @clipboard for the given MIME type @format as text.
 *
 * Returns: (transfer full) (nullable): a new allocated string.
 */
char* wpe_clipboard_read_text(WPEClipboard* clipboard, const char* format, gsize* size)
{
    g_return_val_if_fail(WPE_IS_CLIPBOARD(clipboard), nullptr);
    g_return_val_if_fail(format && *format, nullptr);

    if (auto* bytes = wpe_clipboard_read_bytes(clipboard, format))
        return static_cast<char*>(g_bytes_unref_to_data(bytes, size));

    if (size)
        *size = 0;
    return nullptr;
}

/**
 * wpe_clipboard_content_new:
 *
 * Create a new #WPEClipboardContent
 *
 * Returns: (transfer full): a new #WPEClipboardContent
 */
WPEClipboardContent* wpe_clipboard_content_new(void)
{
    auto* content = static_cast<WPEClipboardContent*>(fastMalloc(sizeof(WPEClipboardContent)));
    new (content) WPEClipboardContent();
    return content;
}

/**
 * wpe_clipboard_content_ref:
 * @content: a #WPEClipboardContent
 *
 * Atomically acquires a reference on the given @content.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The same @content with an additional reference.
 */
WPEClipboardContent* wpe_clipboard_content_ref(WPEClipboardContent* content)
{
    g_return_val_if_fail(content, nullptr);

    g_atomic_int_inc(&content->referenceCount);
    return content;
}

/**
 * wpe_clipboard_content_unref:
 * @content: a #WPEClipboardContent
 *
 * Atomically releases a reference on the given @content.
 *
 * If the reference was the last, the resources associated to the
 * @content are freed. This function is MT-safe and may be called from
 * any thread.
 */
void wpe_clipboard_content_unref(WPEClipboardContent* content)
{
    g_return_if_fail(content);

    if (g_atomic_int_dec_and_test(&content->referenceCount)) {
        content->~WPEClipboardContent();
        fastFree(content);
    }
}

/**
 * wpe_clipboard_content_set_text:
 * @content: a #WPEClipboardContent
 * @text: the text to set
 *
 * Set @text on @content.
 */
void wpe_clipboard_content_set_text(WPEClipboardContent* content, const char* text)
{
    g_return_if_fail(content);

    content->text = CString(text);
}

/**
 * wpe_clipboard_content_get_text:
 * @content: a #WPEClipboardContent
 *
 * Get the text on @content.
 *
 * Returns: (transfer none) (nullable): the text on @content or %NULL
 */
const char* wpe_clipboard_content_get_text(WPEClipboardContent* content)
{
    g_return_val_if_fail(content, nullptr);

    return content->text ? content->text->data() : nullptr;
}

/**
 * wpe_clipboard_content_set_bytes:
 * @content: a #WPEClipboardContent
 * @format: a MIME type format
 * @bytes: a #GBytes with the data to set
 *
 * Set @bytes data on @content for MIME type @format.
 */
void wpe_clipboard_content_set_bytes(WPEClipboardContent* content, const char* format, GBytes* bytes)
{
    g_return_if_fail(content);
    g_return_if_fail(format && *format);

    if (!content->buffers)
        content->buffers = HashMap<const char*, GRefPtr<GBytes>>();
    content->buffers->add(g_intern_string(format), bytes);
}

/**
 * wpe_clipboard_content_get_bytes:
 * @content: a #WPEClipboardContent
 * @format: a MIME type format
 *
 * Get a #GBytes with data on @content for MIME type @format.
 *
 * Returns: (transfer none) (nullable): the bytes on @content for MIME type @format or %NULL
 */
GBytes* wpe_clipboard_content_get_bytes(WPEClipboardContent* content, const char* format)
{
    g_return_val_if_fail(content, nullptr);
    g_return_val_if_fail(format && *format, nullptr);

    if (!content->buffers)
        return nullptr;

    return content->buffers->get(g_intern_string(format));
}

/**
 * wpe_clipboard_content_serialize:
 * @content: a #WPEClipboardContent
 * @format: a MIME type format
 * @stream: a #GOutputStream
 *
 * Serialize @content for MIME type @format in @stream.
 *
 * Returns: %TRUE if content was correctly serialized, or %FALSE otherwise
 */
gboolean wpe_clipboard_content_serialize(WPEClipboardContent* content, const char* format, GOutputStream* stream)
{
    g_return_val_if_fail(content, FALSE);
    g_return_val_if_fail(format && *format, FALSE);
    g_return_val_if_fail(G_IS_OUTPUT_STREAM(stream), FALSE);

    const auto* internalFormat = g_intern_string(format);
    if (internalFormat == g_intern_static_string("text/plain") || internalFormat == g_intern_static_string("text/plain;charset=utf-8")) {
        if (content->text)
            return g_output_stream_write_all(stream, content->text->data(), content->text->length(), nullptr, nullptr, nullptr);
    } else if (content->buffers) {
        if (auto* bytes = content->buffers->get(internalFormat)) {
            gsize dataLength;
            const auto* data = g_bytes_get_data(bytes, &dataLength);
            return g_output_stream_write_all(stream, data, dataLength, nullptr, nullptr, nullptr);
        }
    }

    return FALSE;
}
