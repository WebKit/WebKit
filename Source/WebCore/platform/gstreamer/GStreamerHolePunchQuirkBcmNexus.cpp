/*
 * Copyright (C) 2024 Igalia S.L
 * Copyright (C) 2024 Metrological Group B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GStreamerHolePunchQuirkBcmNexus.h"

#include "GStreamerCommon.h"
#include <wtf/text/MakeString.h>

#if USE(GSTREAMER)

namespace WebCore {

bool GStreamerHolePunchQuirkBcmNexus::setHolePunchVideoRectangle(GstElement* videoSink, const IntRect& rect)
{
    if (UNLIKELY(!gstObjectHasProperty(videoSink, "rectangle")))
        return false;

    auto rectString = makeString(rect.x(), ',', rect.y(), ',', rect.width(), ',', rect.height());
    g_object_set(videoSink, "rectangle", rectString.ascii().data(), nullptr);
    return true;
}

} // namespace WebCore

#endif // USE(GSTREAMER)
