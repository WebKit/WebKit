/*
 * Copyright (C) 2017 Igalia S.L.
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

#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>

namespace ImageDiff {

std::unique_ptr<PlatformImage> PlatformImage::createFromStdin(size_t)
{
    cairo_surface_t* surface = cairo_image_surface_create_from_png_stream(
        [](void*, unsigned char* data, unsigned length) -> cairo_status_t {
            size_t readBytes = fread(data, 1, length, stdin);
            return readBytes == length ? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_READ_ERROR;
        }, nullptr);

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surface);
        return nullptr;
    }

    return std::make_unique<PlatformImage>(surface);
}

std::unique_ptr<PlatformImage> PlatformImage::createFromDiffData(void* data, size_t width, size_t height)
{
    cairo_surface_t* surface = cairo_image_surface_create_for_data(reinterpret_cast<unsigned char*>(data), CAIRO_FORMAT_A8,
        width, height, cairo_format_stride_for_width(CAIRO_FORMAT_A8, width));
    static cairo_user_data_key_t imageDataKey;
    cairo_surface_set_user_data(surface, &imageDataKey, data, [](void* data) { free(data); });
    return std::make_unique<PlatformImage>(surface);
}

PlatformImage::PlatformImage(cairo_surface_t* surface)
    : m_image(surface)
{
}

PlatformImage::~PlatformImage()
{
    cairo_surface_destroy(m_image);
}

size_t PlatformImage::width() const
{
    return cairo_image_surface_get_width(m_image);
}

size_t PlatformImage::height() const
{
    return cairo_image_surface_get_height(m_image);
}

size_t PlatformImage::rowBytes() const
{
    return cairo_image_surface_get_stride(m_image);
}

bool PlatformImage::hasAlpha() const
{
    // What matters here is whether the image data has an alpha channel. In cairo, both
    // CAIRO_FORMAT_ARGB32 and CAIRO_FORMAT_RGB24 have an alpha channel even if it's
    // always 0 in the CAIRO_FORMAT_RGB24 case.
    return cairo_image_surface_get_format(m_image) == CAIRO_FORMAT_ARGB32 || cairo_image_surface_get_format(m_image) == CAIRO_FORMAT_RGB24;
}

unsigned char* PlatformImage::pixels() const
{
    return cairo_image_surface_get_data(m_image);
}

void PlatformImage::writeAsPNGToStdout()
{
    struct WriteContext {
        unsigned long writtenBytes { 0 };
    } context;

    // First we sum up the bytes that are to be written.
    cairo_surface_write_to_png_stream(m_image,
        [](void* closure, const unsigned char*, unsigned length) -> cairo_status_t {
            auto& context = *static_cast<WriteContext*>(closure);
            context.writtenBytes += length;
            return CAIRO_STATUS_SUCCESS;
        }, &context);
    fprintf(stdout, "Content-Length: %lu\n", context.writtenBytes);
    cairo_surface_write_to_png_stream(m_image,
        [](void*, const unsigned char* data, unsigned length) -> cairo_status_t {
            size_t writtenBytes = fwrite(data, 1, length, stdout);
            return writtenBytes == length ? CAIRO_STATUS_SUCCESS : CAIRO_STATUS_WRITE_ERROR;
        }, nullptr);
}

} // namespace ImageDiff
