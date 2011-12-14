/*
 *  Copyright (C) 2011 Igalia S.L
 *
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
#include "GRefPtrGStreamer.h"

#if USE(GSTREAMER)
#include <gst/gstelement.h>

namespace WTF {

template <> GRefPtr<GstElement> adoptGRef(GstElement* ptr)
{
    ASSERT(!ptr || !GST_OBJECT_IS_FLOATING(GST_OBJECT(ptr)));
    return GRefPtr<GstElement>(ptr, GRefPtrAdopt);
}

template <> GstElement* refGPtr<GstElement>(GstElement* ptr)
{
    if (ptr) {
        gst_object_ref(GST_OBJECT(ptr));
        gst_object_sink(GST_OBJECT(ptr));
    }

    return ptr;
}

template <> void derefGPtr<GstElement>(GstElement* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}

template <> GRefPtr<GstPad> adoptGRef(GstPad* ptr)
{
    ASSERT(!ptr || !GST_OBJECT_IS_FLOATING(GST_OBJECT(ptr)));
    return GRefPtr<GstPad>(ptr, GRefPtrAdopt);
}

template <> GstPad* refGPtr<GstPad>(GstPad* ptr)
{
    if (ptr) {
        gst_object_ref(GST_OBJECT(ptr));
        gst_object_sink(GST_OBJECT(ptr));
    }
    return ptr;
}

template <> void derefGPtr<GstPad>(GstPad* ptr)
{
    if (ptr)
        gst_object_unref(GST_OBJECT(ptr));
}

template <> GRefPtr<GstPadTemplate> adoptGRef(GstPadTemplate* ptr)
{
    ASSERT(!ptr || !GST_OBJECT_IS_FLOATING(GST_OBJECT(ptr)));
    return GRefPtr<GstPadTemplate>(ptr, GRefPtrAdopt);
}

template <> GstPadTemplate* refGPtr<GstPadTemplate>(GstPadTemplate* ptr)
{
    if (ptr) {
        gst_object_ref(GST_OBJECT(ptr));
        gst_object_sink(GST_OBJECT(ptr));
    }
    return ptr;
}

template <> void derefGPtr<GstPadTemplate>(GstPadTemplate* ptr)
{
    if (ptr)
        gst_object_unref(GST_OBJECT(ptr));
}

template <> GstCaps* refGPtr<GstCaps>(GstCaps* ptr)
{
    if (ptr)
        gst_caps_ref(ptr);
    return ptr;
}

template <> void derefGPtr<GstCaps>(GstCaps* ptr)
{
    if (ptr)
        gst_caps_unref(ptr);
}


template <> GRefPtr<GstTask> adoptGRef(GstTask* ptr)
{
    ASSERT(!GST_OBJECT_IS_FLOATING(GST_OBJECT(ptr)));
    return GRefPtr<GstTask>(ptr, GRefPtrAdopt);
}

template <> GstTask* refGPtr<GstTask>(GstTask* ptr)
{
    if (ptr) {
        gst_object_ref(GST_OBJECT(ptr));
        gst_object_sink(GST_OBJECT(ptr));
    }

    return ptr;
}

template <> void derefGPtr<GstTask>(GstTask* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}

}
#endif // USE(GSTREAMER)
