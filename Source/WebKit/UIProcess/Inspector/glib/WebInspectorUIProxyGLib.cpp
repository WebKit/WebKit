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

#include "config.h"
#include "WebInspectorUIProxy.h"

#include <gio/gio.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/Base64.h>

namespace WebKit {

static void fileReplaceContentsCallback(GObject* sourceObject, GAsyncResult* result, gpointer)
{
    GFile* file = G_FILE(sourceObject);
    g_file_replace_contents_finish(file, result, nullptr, nullptr);
}

void WebInspectorUIProxy::platformSaveDataToFile(GRefPtr<GFile>&& file, const String& content, bool base64Encoded)
{
    GUniqueOutPtr<GError> error;

    Vector<uint8_t> dataVector;
    CString dataString;
    if (base64Encoded) {
        auto decodedData = base64Decode(content);
        if (!decodedData)
            return;
        decodedData->shrinkToFit();
        dataVector = WTF::move(*decodedData);
    } else
        dataString = content.utf8();

    const char* data = !dataString.isNull() ? dataString.data() : reinterpret_cast<const char*>(dataVector.span().data());
    size_t dataLength = !dataString.isNull() ? dataString.length() : dataVector.size();

    g_file_replace_contents_async(file.get(), data, dataLength, nullptr, false,
        G_FILE_CREATE_NONE, nullptr, fileReplaceContentsCallback, protect(inspectorPage()).get());

    if (error)
        g_warning("Failed to save inspector data: %s", error->message);
}

} // namespace WebKit
