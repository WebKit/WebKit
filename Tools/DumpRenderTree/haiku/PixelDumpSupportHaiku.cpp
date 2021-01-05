/*
 * Copyright (C) 2013 Haiku, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PixelDumpSupportHaiku.h"

#include "DumpRenderTree.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "PixelDumpSupport.h"
#include "WebView.h"

#include <DumpRenderTreeClient.h>

#include <wtf/SHA1.h>

#include <Bitmap.h>
#include <BitmapStream.h>
#include <DataIO.h>
#include <Rect.h>
#include <TranslatorRoster.h>

extern BWebView* webView;

RefPtr<BitmapContext> createBitmapContextFromWebView(bool, bool, bool, bool drawSelectionRect)
{
    BSize size;
    webView->LockLooper();
    BRect r = webView->Bounds();
    webView->UnlockLooper();
    size.width = r.Width() + 1;
    size.height = r.Height() + 1;
    BBitmap* bitmap = WebCore::DumpRenderTreeClient::getOffscreen(webView);
    return BitmapContext::createByAdoptingData(size, bitmap);
}

void computeSHA1HashStringForBitmapContext(BitmapContext* context, char hashString[33])
{
    if (!context || !context->m_bitmap)
        return;

    BRect bounds = context->m_bitmap->Bounds();
    int pixelsWide = bounds.Width();
    int pixelsHigh = bounds.Height();
    int bytesPerRow = context->m_bitmap->BytesPerRow();
    unsigned char* pixelData = (unsigned char*)context->m_bitmap->Bits();

    SHA1 sha1;
    for (int i = 0; i <= pixelsHigh; ++i) {
        sha1.addBytes(pixelData, 4 * pixelsWide);
        pixelData += bytesPerRow;
    }

    SHA1::Digest hash;
    sha1.computeHash(hash);

    snprintf(hashString, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
        hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6], hash[7],
        hash[8], hash[9], hash[10], hash[11], hash[12], hash[13], hash[14], hash[15]);
}

void dumpBitmap(BitmapContext* context, const char* checksum)
{
    if (!context || !context->m_bitmap)
        return;

    BBitmapStream stream(context->m_bitmap);
    BMallocIO mio;
    status_t err = BTranslatorRoster::Default()->Translate(&stream, NULL, NULL, &mio, B_PNG_FORMAT);
    if (err == B_OK)
        printPNG((const unsigned char*)mio.Buffer(), mio.BufferLength(), checksum);
    else
        fprintf(stderr, "Error translating bitmap: %s\n", strerror(err));

    BBitmap* out;
    stream.DetachBitmap(&out);
    ASSERT(out == context->m_bitmap);
}
