/*
 * Copyright (C) 2021 Igalia S.L.
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
#include "WebKitDirectoryInputStream.h"

#include "WebKitDirectoryInputStreamData.h"
#include <glib/gi18n-lib.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

struct _WebKitDirectoryInputStreamPrivate {
    GRefPtr<GFileEnumerator> enumerator;
    CString uri;

    GRefPtr<GBytes> buffer;
    bool readDone;
};

WEBKIT_DEFINE_TYPE(WebKitDirectoryInputStream, webkit_directory_input_stream, G_TYPE_INPUT_STREAM)

static GBytes* webkitDirectoryInputStreamCreateHeader(WebKitDirectoryInputStream *stream)
{
    char* header = g_strdup_printf(
        "<html><head>"
        "<title>%s</title>"
        "<meta http-equiv=\"Content-Type\" content=\"text/html;\" charset=\"UTF-8\">"
        "<style>%s</style>"
        "<script>%s</script>"
        "</head>"
        "<body>"
        "<table>"
        "<thead>"
        "<th align=\"left\">%s</th><th align=\"right\">%s</th><th align=\"right\">%s</th>"
        "</thead>",
        stream->priv->uri.data(),
        WebCore::directoryUserAgentStyleSheet,
        WebCore::directoryJavaScript,
        _("Name"),
        _("Size"),
        _("Date Modified"));

    return g_bytes_new_with_free_func(header, strlen(header), g_free, header);
}

static GBytes* webkitDirectoryInputStreamCreateFooter(WebKitDirectoryInputStream *stream)
{
    static const char* footer = "</table></body></html>";
    return g_bytes_new_static(footer, strlen(footer));
}

static GBytes* webkitDirectoryInputStreamCreateRow(WebKitDirectoryInputStream *stream, GFileInfo* info)
{
    if (!g_file_info_get_name(info))
        return nullptr;

    const char* name = g_file_info_get_display_name(info);
    if (!name) {
        name = g_file_info_get_name(info);
        if (!g_utf8_validate(name, -1, nullptr))
            return nullptr;
    }

    GUniquePtr<char> markupName(g_markup_escape_text(name, -1));
    GUniquePtr<char> escapedName(g_uri_escape_string(name, nullptr, FALSE));
    GUniquePtr<char> path(g_build_filename(stream->priv->uri.data(), escapedName.get(), nullptr));
    GUniquePtr<char> formattedSize(g_file_info_get_file_type(info) == G_FILE_TYPE_REGULAR ? g_format_size(g_file_info_get_size(info)) : nullptr);
    GUniquePtr<char> formattedName(g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY ? g_strdup_printf("1.%s", path.get()) : g_strdup_printf("%s", path.get()));
#if GLIB_CHECK_VERSION (2, 61, 2)
    GRefPtr<GDateTime> modificationTime = adoptGRef(g_file_info_get_modification_date_time(info));
#else
    GTimeVal modified;
    g_file_info_get_modification_time(info, &modified);
    GRefPtr<GDateTime> modificationTime = adoptGRef(g_date_time_new_from_timeval_local(&modified));
#endif
    GUniquePtr<char> formattedTime(g_date_time_format(modificationTime.get(), "%X"));
    GUniquePtr<char> formattedDate(g_date_time_format(modificationTime.get(), "%x"));

    char* row = g_strdup_printf(
        "<tr>"
        "<td sortable-data=\"%s\"><a href=\"%s\">%s</a></td>"
        "<td align=\"right\" sortable-data=\"%" G_GOFFSET_FORMAT "\">%s</td>"
        "<td align=\"right\" sortable-data=\"%" G_GINT64_FORMAT "\">%s&ensp;%s</td>\n"
        "</tr>",
        formattedName.get(), path.get(), markupName.get(), g_file_info_get_size(info),
        formattedSize ? formattedSize.get() : "", g_date_time_to_unix(modificationTime.get()), formattedTime.get(), formattedDate.get());
    return g_bytes_new_with_free_func(row, strlen(row), g_free, row);
}

static GBytes* webkitDirectoryInputStreamReadNextFile(WebKitDirectoryInputStream* stream, GCancellable* cancellable, GError** error)
{
    GBytes* buffer = nullptr;
    do {
        GError* fileError = nullptr;
        GRefPtr<GFileInfo> info = adoptGRef(g_file_enumerator_next_file(stream->priv->enumerator.get(), cancellable, &fileError));
        if (fileError) {
            g_propagate_error(error, fileError);
            return nullptr;
        }

        if (!info && !stream->priv->readDone) {
            stream->priv->readDone = true;
            buffer = webkitDirectoryInputStreamCreateFooter(stream);
        } else if (info)
            buffer = webkitDirectoryInputStreamCreateRow(stream, info.get());
    } while (!buffer && !stream->priv->readDone);

    return buffer;
}

static gssize webkitDirectoryInputStreamRead(GInputStream* input, void* buffer, gsize count, GCancellable* cancellable, GError** error)
{
    auto* stream = WEBKIT_DIRECTORY_INPUT_STREAM(input);

    if (stream->priv->readDone)
        return 0;

    gsize totalBytesRead = 0;
    while (totalBytesRead < count) {
        if (!stream->priv->buffer) {
            stream->priv->buffer = adoptGRef(webkitDirectoryInputStreamReadNextFile(stream, cancellable, error));
            if (!stream->priv->buffer) {
                if (totalBytesRead)
                    g_clear_error(error);
                return totalBytesRead;
            }
        }

        gsize bufferSize;
        auto* bufferData = g_bytes_get_data(stream->priv->buffer.get(), &bufferSize);
        gsize bytesRead = std::min(bufferSize, count - totalBytesRead);
        memcpy(static_cast<char*>(buffer) + totalBytesRead, bufferData, bytesRead);
        if (bytesRead == bufferSize)
            stream->priv->buffer = nullptr;
        else
            stream->priv->buffer = g_bytes_new_from_bytes(stream->priv->buffer.get(), bytesRead, bufferSize - bytesRead);
        totalBytesRead += bytesRead;
    }

    return totalBytesRead;
}

static gboolean webkitDirectoryInputStreamClose(GInputStream* input, GCancellable* cancellable, GError** error)
{
    auto* priv = WEBKIT_DIRECTORY_INPUT_STREAM(input)->priv;

    priv->buffer = nullptr;

    return g_file_enumerator_close(priv->enumerator.get(), cancellable, error);
}

static void
webkit_directory_input_stream_class_init(WebKitDirectoryInputStreamClass* klass)
{
    auto* inputStreamClass = G_INPUT_STREAM_CLASS(klass);
    inputStreamClass->read_fn = webkitDirectoryInputStreamRead;
    inputStreamClass->close_fn = webkitDirectoryInputStreamClose;
}

GRefPtr<GInputStream> webkitDirectoryInputStreamNew(GRefPtr<GFileEnumerator>&& enumerator, CString&& uri)
{
    auto* stream = WEBKIT_DIRECTORY_INPUT_STREAM(g_object_new(WEBKIT_TYPE_DIRECTORY_INPUT_STREAM, nullptr));
    stream->priv->enumerator = WTFMove(enumerator);
    stream->priv->uri = WTFMove(uri);
    stream->priv->buffer = adoptGRef(webkitDirectoryInputStreamCreateHeader(stream));

    return adoptGRef(G_INPUT_STREAM((stream)));
}
