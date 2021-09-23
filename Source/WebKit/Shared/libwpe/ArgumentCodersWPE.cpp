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
#include "ArgumentCodersWPE.h"

#include "DataReference.h"
#include "ShareableBitmap.h"
#include "WebCoreArgumentCoders.h"
#include <WebCore/GraphicsContext.h>
#include <WebCore/Image.h>
#include <WebCore/SelectionData.h>

namespace IPC {
using namespace WebCore;
using namespace WebKit;

static void encodeImage(Encoder& encoder, Image& image)
{
    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::createShareable(IntSize(image.size()), { });
    bitmap->createGraphicsContext()->drawImage(image, IntPoint());

    ShareableBitmap::Handle handle;
    bitmap->createHandle(handle);

    encoder << handle;
}

static WARN_UNUSED_RETURN bool decodeImage(Decoder& decoder, RefPtr<Image>& image)
{
    ShareableBitmap::Handle handle;
    if (!decoder.decode(handle))
        return false;

    RefPtr<ShareableBitmap> bitmap = ShareableBitmap::create(handle);
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
        encoder << RefPtr<SharedBuffer>(selection.customData());

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
        selection.setURL(URL(URL(), url), String());
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

}
