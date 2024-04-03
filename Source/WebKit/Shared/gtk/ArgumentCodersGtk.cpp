/*
 * Copyright (C) 2011 Igalia S.L.
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
#include "ArgumentCodersGtk.h"

#include "WebCoreArgumentCoders.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/Image.h>
#include <WebCore/SelectionData.h>
#include <WebCore/ShareableBitmap.h>
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

namespace IPC {
using namespace WebCore;
using namespace WebKit;

static void encodeGKeyFile(Encoder& encoder, GKeyFile* keyFile)
{
    gsize dataSize;
    GUniquePtr<char> data(g_key_file_to_data(keyFile, &dataSize, 0));
    encoder << std::span<const uint8_t>(reinterpret_cast<uint8_t*>(data.get()), dataSize);
}

static WARN_UNUSED_RETURN bool decodeGKeyFile(Decoder& decoder, GUniquePtr<GKeyFile>& keyFile)
{
    std::span<const uint8_t> dataReference;
    if (!decoder.decode(dataReference))
        return false;

    if (!dataReference.size())
        return true;

    keyFile.reset(g_key_file_new());
    if (!g_key_file_load_from_data(keyFile.get(), reinterpret_cast<const gchar*>(dataReference.data()), dataReference.size(), G_KEY_FILE_NONE, 0)) {
        keyFile.reset();
        return false;
    }

    return true;
}

void ArgumentCoder<GRefPtr<GtkPrintSettings>>::encode(Encoder& encoder, const GRefPtr<GtkPrintSettings>& argument)
{
    GRefPtr<GtkPrintSettings> printSettings = argument ? argument : adoptGRef(gtk_print_settings_new());
    GUniquePtr<GKeyFile> keyFile(g_key_file_new());
    gtk_print_settings_to_key_file(printSettings.get(), keyFile.get(), "Print Settings");
    encodeGKeyFile(encoder, keyFile.get());
}

std::optional<GRefPtr<GtkPrintSettings>> ArgumentCoder<GRefPtr<GtkPrintSettings>>::decode(Decoder& decoder)
{
    GUniquePtr<GKeyFile> keyFile;
    if (!decodeGKeyFile(decoder, keyFile))
        return std::nullopt;

    GRefPtr<GtkPrintSettings> printSettings = adoptGRef(gtk_print_settings_new());
    if (!keyFile)
        return printSettings;

    if (!gtk_print_settings_load_key_file(printSettings.get(), keyFile.get(), "Print Settings", nullptr))
        return std::nullopt;

    return printSettings;
}

void ArgumentCoder<GRefPtr<GtkPageSetup>>::encode(Encoder& encoder, const GRefPtr<GtkPageSetup>& argument)
{
    GRefPtr<GtkPageSetup> pageSetup = argument ? argument : adoptGRef(gtk_page_setup_new());
    GUniquePtr<GKeyFile> keyFile(g_key_file_new());
    gtk_page_setup_to_key_file(pageSetup.get(), keyFile.get(), "Page Setup");
    encodeGKeyFile(encoder, keyFile.get());
}

std::optional<GRefPtr<GtkPageSetup>> ArgumentCoder<GRefPtr<GtkPageSetup>>::decode(Decoder& decoder)
{
    GUniquePtr<GKeyFile> keyFile;
    if (!decodeGKeyFile(decoder, keyFile))
        return std::nullopt;

    GRefPtr<GtkPageSetup> pageSetup = adoptGRef(gtk_page_setup_new());
    if (!keyFile)
        return pageSetup;

    if (!gtk_page_setup_load_key_file(pageSetup.get(), keyFile.get(), "Page Setup", nullptr))
        return std::nullopt;

    return pageSetup;
}

}
