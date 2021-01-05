/*
 * Copyright (C) 2013-2019 Haiku, inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "PlatformImage.h"

#include <interface/Bitmap.h>
#include <BitmapStream.h>
#include <TranslationUtils.h>
#include <TranslatorRoster.h>

#include <string.h>


namespace ImageDiff {

std::unique_ptr<PlatformImage> PlatformImage::createFromStdin(size_t imageSize)
{
    auto imageBuffer = new unsigned char[imageSize];
    if (!imageBuffer)
		return nullptr;

    const size_t bytesRead = fread(imageBuffer, 1, imageSize, stdin);
    if (bytesRead <= 0) {
        delete[] imageBuffer;
		return nullptr;
    }

    BMemoryIO imageData(imageBuffer, imageSize);
    BBitmap* image = BTranslationUtils::GetBitmap(&imageData);

    delete[] imageBuffer;

    if (image == NULL)
		return nullptr;

    return std::make_unique<PlatformImage>(image);
}


std::unique_ptr<PlatformImage> PlatformImage::createFromDiffData(void* data, size_t width, size_t height)
{
	BBitmap* surface = new BBitmap(BRect(0, 0, width - 1, height - 1), B_GRAY8);
	void* dest = surface->Bits();
	memcpy(dest, data, width * height);
	free(data);
    return std::make_unique<PlatformImage>(surface);
}


PlatformImage::PlatformImage(BBitmap* surface)
    : m_image(surface)
{
}


PlatformImage::~PlatformImage()
{
    delete m_image;
}

size_t PlatformImage::width() const
{
    return m_image->Bounds().Width() + 1;
}

size_t PlatformImage::height() const
{
    return m_image->Bounds().Height() + 1;
}

size_t PlatformImage::rowBytes() const
{
    return m_image->BytesPerRow();
}


bool PlatformImage::hasAlpha() const
{
	return m_image->ColorSpace() == B_RGBA32;
}

unsigned char* PlatformImage::pixels() const
{
	return (unsigned char*)m_image->Bits();
}


void PlatformImage::writeAsPNGToStdout()
{
    BMallocIO imageData;
    BBitmapStream input(m_image);
    if (BTranslatorRoster::Default()->Translate(&input, NULL, NULL,
        &imageData, B_PNG_FORMAT) == B_OK) {
        printf("Content-Length: %ld\n", imageData.BufferLength());
        fwrite(imageData.Buffer(), 1, imageData.BufferLength(), stdout);
        fflush(stdout);
    }
	BBitmap* foo = NULL;
	input.DetachBitmap(&foo);
}

} // namespace ImageDiff
