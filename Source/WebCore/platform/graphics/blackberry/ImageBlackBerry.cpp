/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "Image.h"

#include "BitmapImage.h"
#include "ImageBuffer.h"
#include "SharedBuffer.h"

namespace WebCore {

PassRefPtr<Image> Image::loadPlatformResource(const char *name)
{
    if (!strcmp(name, "searchCancel") || !strcmp(name, "searchCancelPressed")) {
        OwnPtr<ImageBuffer> imageBuffer = ImageBuffer::create(IntSize(16, 16));
        if (!imageBuffer)
            return 0;

        // Draw a more subtle, gray x-shaped icon.
        GraphicsContext* context = imageBuffer->context();
        context->save();

        context->fillRect(FloatRect(0, 0, 16, 16), Color::white, ColorSpaceDeviceRGB);

        if (!strcmp(name, "searchCancel"))
            context->setFillColor(Color(128, 128, 128), ColorSpaceDeviceRGB);
        else
            context->setFillColor(Color(64, 64, 64), ColorSpaceDeviceRGB);

        context->translate(8, 8);

        context->rotate(piDouble / 4.0);
        context->fillRect(FloatRect(-1, -7, 2, 14));

        context->rotate(-piDouble / 2.0);
        context->fillRect(FloatRect(-1, -7, 2, 14));

        context->restore();
        return imageBuffer->copyImage();
    }

    // RESOURCE_PATH is set by CMake in OptionsBlackBerry.cmake
    String fullPath(RESOURCE_PATH);
    String extension(".png");

    fullPath += name;
    fullPath += extension;

    RefPtr<SharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(fullPath);
    if (!buffer)
        return BitmapImage::nullImage();

    RefPtr<BitmapImage> img = BitmapImage::create();
    img->setData(buffer.release(), true /* allDataReceived */);
    return img.release();
}

} // namespace WebCore
