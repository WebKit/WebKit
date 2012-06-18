/*
 * Copyright (C) 2012 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "GStreamerVersioning.h"

#include <gst/gst.h>

void webkitGstObjectRefSink(GstObject* gstObject)
{
#ifdef GST_API_VERSION_1
    gst_object_ref_sink(gstObject);
#else
    gst_object_ref(gstObject);
    gst_object_sink(gstObject);
#endif
}

GstCaps* webkitGstGetPadCaps(GstPad* pad)
{
    if (!pad)
        return 0;

    GstCaps* caps;
#ifdef GST_API_VERSION_1
    caps = gst_pad_get_current_caps(pad);
    if (!caps)
        caps = gst_pad_query_caps(pad, 0);
#else
    caps = GST_PAD_CAPS(pad);
#endif
    return caps;
}
