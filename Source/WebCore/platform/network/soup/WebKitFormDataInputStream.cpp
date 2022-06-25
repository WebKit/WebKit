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
#include "WebKitFormDataInputStream.h"

#include "BlobData.h"
#include <wtf/FileSystem.h>
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

struct _WebKitFormDataInputStreamPrivate {
    RefPtr<FormData> formData;
    GRefPtr<GInputStream> currentStream;
    unsigned nextIndex;
    long long currentStreamRangeLength;
    bool canPoll;
};

static void webkitFormDataInputStreamPollableInterfaceInit(GPollableInputStreamInterface*);

WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitFormDataInputStream, webkit_form_data_input_stream, G_TYPE_INPUT_STREAM,
    G_IMPLEMENT_INTERFACE(G_TYPE_POLLABLE_INPUT_STREAM, webkitFormDataInputStreamPollableInterfaceInit))

static bool webkitFormDataInputStreamCreateNextStream(WebKitFormDataInputStream* stream, GCancellable* cancellable)
{
    auto* priv = stream->priv;
    if (priv->currentStream) {
        g_input_stream_close(G_INPUT_STREAM(priv->currentStream.get()), cancellable, nullptr);
        priv->currentStream = nullptr;
        priv->currentStreamRangeLength = BlobDataItem::toEndOfFile;
    }

    const auto& elements = priv->formData->elements();
    if (elements.size() == priv->nextIndex)
        return true;

    const auto& element = elements[priv->nextIndex++];
    switchOn(element.data,
        [priv] (const Vector<uint8_t>& data) {
            GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new_static(data.data(), data.size()));
            priv->currentStream = adoptGRef(g_memory_input_stream_new_from_bytes(bytes.get()));
        }, [priv, cancellable] (const FormDataElement::EncodedFileData& fileData) {
            if (fileData.fileModificationTimeMatchesExpectation()) {
                GRefPtr<GFile> file = adoptGRef(g_file_new_for_path(FileSystem::fileSystemRepresentation(fileData.filename).data()));
                priv->currentStream = adoptGRef(G_INPUT_STREAM(g_file_read(file.get(), cancellable, nullptr)));
                if (G_IS_SEEKABLE(priv->currentStream.get()) && fileData.fileStart > 0)
                    g_seekable_seek(G_SEEKABLE(priv->currentStream.get()), fileData.fileStart, G_SEEK_SET, cancellable, nullptr);
                if (priv->currentStream)
                    priv->currentStreamRangeLength = fileData.fileLength;
            }
        }, [] (const FormDataElement::EncodedBlobData&) {
            ASSERT_NOT_REACHED();
        }
    );

    return !!priv->currentStream;
}

static gssize webkitFormDataInputStreamRead(GInputStream* input, void* buffer, gsize count, GCancellable* cancellable, GError** error)
{
    auto* stream = WEBKIT_FORM_DATA_INPUT_STREAM(input);
    auto* priv = stream->priv;
    if (!priv->currentStream)
        while (!webkitFormDataInputStreamCreateNextStream(stream, cancellable)) { }

    while (priv->currentStream) {
        auto bytesToRead = count;
        if (priv->currentStreamRangeLength != BlobDataItem::toEndOfFile && static_cast<unsigned long long>(priv->currentStreamRangeLength) < count)
            bytesToRead = priv->currentStreamRangeLength;

        auto bytesRead = g_input_stream_read(priv->currentStream.get(), buffer, bytesToRead, cancellable, error);
        if (bytesRead == -1)
            return bytesRead;

        if (bytesRead) {
            if (priv->currentStreamRangeLength != BlobDataItem::toEndOfFile)
                priv->currentStreamRangeLength -= bytesRead;
            return bytesRead;
        }

        while (!webkitFormDataInputStreamCreateNextStream(stream, cancellable)) { }
    }

    return 0;
}

static gboolean webkitFormDataInputStreamClose(GInputStream* input, GCancellable* cancellable, GError**)
{
    auto* priv = WEBKIT_FORM_DATA_INPUT_STREAM(input)->priv;
    if (priv->currentStream) {
        g_input_stream_close(G_INPUT_STREAM(priv->currentStream.get()), cancellable, nullptr);
        priv->currentStream = nullptr;
        priv->currentStreamRangeLength = BlobDataItem::toEndOfFile;
    }
    priv->nextIndex = priv->formData->elements().size();

    return TRUE;
}

static void webkit_form_data_input_stream_class_init(WebKitFormDataInputStreamClass* klass)
{
    auto* inputStreamClass = G_INPUT_STREAM_CLASS(klass);
    inputStreamClass->read_fn = webkitFormDataInputStreamRead;
    inputStreamClass->close_fn = webkitFormDataInputStreamClose;
}

GRefPtr<GInputStream> webkitFormDataInputStreamNew(Ref<FormData>&& formData)
{
    auto* stream = WEBKIT_FORM_DATA_INPUT_STREAM(g_object_new(WEBKIT_TYPE_FORM_DATA_INPUT_STREAM, nullptr));
    stream->priv->formData = WTFMove(formData);
    stream->priv->currentStreamRangeLength = BlobDataItem::toEndOfFile;

    // GFileInputStream is not pollable, so the stream is only pollable if FormData doesn't contain EncodedFileData elements.
    stream->priv->canPoll = true;
    for (const auto& element : stream->priv->formData->elements()) {
        if (std::holds_alternative<FormDataElement::EncodedFileData>(element.data)) {
            stream->priv->canPoll = false;
            break;
        }
    }

    return adoptGRef(G_INPUT_STREAM((stream)));
}

GBytes* webkitFormDataInputStreamReadAll(WebKitFormDataInputStream* stream)
{
    g_return_val_if_fail(WEBKIT_IS_FORM_DATA_INPUT_STREAM(stream), nullptr);

    GRefPtr<GOutputStream> outputStream = adoptGRef(g_memory_output_stream_new(nullptr, 0, g_realloc, g_free));
    auto bytesWritten = g_output_stream_splice(outputStream.get(), G_INPUT_STREAM(stream),
        static_cast<GOutputStreamSpliceFlags>(G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET), nullptr, nullptr);
    if (bytesWritten <= 0)
        return nullptr;

    return g_memory_output_stream_steal_as_bytes(G_MEMORY_OUTPUT_STREAM(outputStream.get()));
}

static gboolean webkitFormDataInputStreamCanPoll(GPollableInputStream* stream)
{
    auto* priv = WEBKIT_FORM_DATA_INPUT_STREAM(stream)->priv;
    return priv->canPoll;
}

static gboolean webkitFormDataInputStreamIsReadable(GPollableInputStream* stream)
{
    auto* priv = WEBKIT_FORM_DATA_INPUT_STREAM(stream)->priv;
    return priv->currentStream || priv->nextIndex < priv->formData->elements().size();
}

static GSource* webkitFormDataInputStreamCreateSource(GPollableInputStream* stream, GCancellable* cancellable)
{
    GRefPtr<GSource> base = adoptGRef(g_timeout_source_new(0));
    return g_pollable_source_new_full(stream, base.get(), cancellable);
}

static void webkitFormDataInputStreamPollableInterfaceInit(GPollableInputStreamInterface* iface)
{
    iface->can_poll = webkitFormDataInputStreamCanPoll;
    iface->is_readable = webkitFormDataInputStreamIsReadable;
    iface->create_source = webkitFormDataInputStreamCreateSource;
}
