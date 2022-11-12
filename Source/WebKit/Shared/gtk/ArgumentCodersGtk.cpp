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

#include "DataReference.h"
#include "ShareableBitmap.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/Image.h>
#include <WebCore/SelectionData.h>
#include <gtk/gtk.h>
#include <wtf/glib/GUniquePtr.h>

namespace IPC {
using namespace WebCore;
using namespace WebKit;

static void encodeImage(Encoder& encoder, Image& image)
{
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(IntSize(image.size()), { });
    bitmap->createGraphicsContext()->drawImage(image, IntPoint());
    encoder << bitmap->createHandle();
}

static WARN_UNUSED_RETURN bool decodeImage(Decoder& decoder, RefPtr<Image>& image)
{
    std::optional<std::optional<ShareableBitmapHandle>> handle;
    decoder >> handle;
    if (!handle || !*handle)
        return false;

    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(**handle);
    if (!bitmap)
        return false;
    image = bitmap->createImage();
    if (!image)
        return false;
    return true;
}

void ArgumentCoder<SelectionData>::encode(Encoder& encoder, const SelectionData& selection)
{
    bool hasText = selection.hasText();
    encoder << hasText;
    if (hasText)
        encoder << selection.text();

    bool hasMarkup = selection.hasMarkup();
    encoder << hasMarkup;
    if (hasMarkup)
        encoder << selection.markup();

    bool hasURL = selection.hasURL();
    encoder << hasURL;
    if (hasURL)
        encoder << selection.url().string();

    bool hasURIList = selection.hasURIList();
    encoder << hasURIList;
    if (hasURIList)
        encoder << selection.uriList();

    bool hasImage = selection.hasImage();
    encoder << hasImage;
    if (hasImage)
        encodeImage(encoder, *selection.image());

    bool hasCustomData = selection.hasCustomData();
    encoder << hasCustomData;
    if (hasCustomData)
        encoder << RefPtr<FragmentedSharedBuffer>(selection.customData());

    bool canSmartReplace = selection.canSmartReplace();
    encoder << canSmartReplace;
}

std::optional<SelectionData> ArgumentCoder<SelectionData>::decode(Decoder& decoder)
{
    SelectionData selection;

    bool hasText;
    if (!decoder.decode(hasText))
        return std::nullopt;
    if (hasText) {
        String text;
        if (!decoder.decode(text))
            return std::nullopt;
        selection.setText(text);
    }

    bool hasMarkup;
    if (!decoder.decode(hasMarkup))
        return std::nullopt;
    if (hasMarkup) {
        String markup;
        if (!decoder.decode(markup))
            return std::nullopt;
        selection.setMarkup(markup);
    }

    bool hasURL;
    if (!decoder.decode(hasURL))
        return std::nullopt;
    if (hasURL) {
        String url;
        if (!decoder.decode(url))
            return std::nullopt;
        selection.setURL(URL { url }, String());
    }

    bool hasURIList;
    if (!decoder.decode(hasURIList))
        return std::nullopt;
    if (hasURIList) {
        String uriList;
        if (!decoder.decode(uriList))
            return std::nullopt;
        selection.setURIList(uriList);
    }

    bool hasImage;
    if (!decoder.decode(hasImage))
        return std::nullopt;
    if (hasImage) {
        RefPtr<Image> image;
        if (!decodeImage(decoder, image))
            return std::nullopt;
        selection.setImage(image.get());
    }

    bool hasCustomData;
    if (!decoder.decode(hasCustomData))
        return std::nullopt;
    if (hasCustomData) {
        RefPtr<SharedBuffer> buffer;
        if (!decoder.decode(buffer))
            return std::nullopt;
        selection.setCustomData(Ref<SharedBuffer>(*buffer));
    }

    bool canSmartReplace;
    if (!decoder.decode(canSmartReplace))
        return std::nullopt;
    selection.setCanSmartReplace(canSmartReplace);

    return selection;
}

static void encodeGKeyFile(Encoder& encoder, GKeyFile* keyFile)
{
    gsize dataSize;
    GUniquePtr<char> data(g_key_file_to_data(keyFile, &dataSize, 0));
    encoder << DataReference(reinterpret_cast<uint8_t*>(data.get()), dataSize);
}

static WARN_UNUSED_RETURN bool decodeGKeyFile(Decoder& decoder, GUniquePtr<GKeyFile>& keyFile)
{
    DataReference dataReference;
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

void encode(Encoder& encoder, GtkPrintSettings* settings)
{
    GRefPtr<GtkPrintSettings> printSettings = settings ? settings : adoptGRef(gtk_print_settings_new());
    GUniquePtr<GKeyFile> keyFile(g_key_file_new());
    gtk_print_settings_to_key_file(printSettings.get(), keyFile.get(), "Print Settings");
    encodeGKeyFile(encoder, keyFile.get());
}

bool decode(Decoder& decoder, GRefPtr<GtkPrintSettings>& printSettings)
{
    GUniquePtr<GKeyFile> keyFile;
    if (!decodeGKeyFile(decoder, keyFile))
        return false;

    printSettings = adoptGRef(gtk_print_settings_new());
    if (!keyFile)
        return true;

    if (!gtk_print_settings_load_key_file(printSettings.get(), keyFile.get(), "Print Settings", nullptr))
        printSettings = nullptr;

    return printSettings;
}

void encode(Encoder& encoder, GtkPageSetup* setup)
{
    GRefPtr<GtkPageSetup> pageSetup = setup ? setup : adoptGRef(gtk_page_setup_new());
    GUniquePtr<GKeyFile> keyFile(g_key_file_new());
    gtk_page_setup_to_key_file(pageSetup.get(), keyFile.get(), "Page Setup");
    encodeGKeyFile(encoder, keyFile.get());
}

bool decode(Decoder& decoder, GRefPtr<GtkPageSetup>& pageSetup)
{
    GUniquePtr<GKeyFile> keyFile;
    if (!decodeGKeyFile(decoder, keyFile))
        return false;

    pageSetup = adoptGRef(gtk_page_setup_new());
    if (!keyFile)
        return true;

    if (!gtk_page_setup_load_key_file(pageSetup.get(), keyFile.get(), "Page Setup", nullptr))
        pageSetup = nullptr;

    return pageSetup;
}

}
