/*
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "DragImage.h"

#include "CachedImage.h"
#include "Image.h"

#include <gtk/gtk.h>

namespace WebCore {

IntSize dragImageSize(DragImageRef image)
{
    if (image)
        return IntSize(gdk_pixbuf_get_width(image), gdk_pixbuf_get_height(image));

    return IntSize(0, 0);
}

void deleteDragImage(DragImageRef image)
{
    if (image)
        g_object_unref(image);
}

DragImageRef scaleDragImage(DragImageRef image, FloatSize scale)
{
    if (image) {
        IntSize imageSize = dragImageSize(image);
        GdkPixbuf* scaledImage = gdk_pixbuf_scale_simple(image,
                                                         imageSize.width() * scale.width(),
                                                         imageSize.height() * scale.height(),
                                                         GDK_INTERP_BILINEAR);
        deleteDragImage(image);
        return scaledImage;
    }

    return 0;
}

DragImageRef dissolveDragImageToFraction(DragImageRef image, float)
{
    return image;
}

DragImageRef createDragImageFromImage(Image* image)
{
    return image->getGdkPixbuf();
}

DragImageRef createDragImageIconForCachedImage(CachedImage*)
{
    return 0;
}

}
