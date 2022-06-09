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
#include <gst/gst.h>

#if USE(GSTREAMER_GL)
#include <gst/gl/egl/gsteglimage.h>
#endif

namespace WTF {

template<> GRefPtr<GstMiniObject> adoptGRef(GstMiniObject* ptr)
{
    return GRefPtr<GstMiniObject>(ptr, GRefPtrAdopt);
}

template<> GstMiniObject* refGPtr<GstMiniObject>(GstMiniObject* ptr)
{
    if (ptr)
        gst_mini_object_ref(ptr);

    return ptr;
}

template<> void derefGPtr<GstMiniObject>(GstMiniObject* ptr)
{
    if (ptr)
        gst_mini_object_unref(ptr);
}

template <> GRefPtr<GstElement> adoptGRef(GstElement* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(ptr));
    return GRefPtr<GstElement>(ptr, GRefPtrAdopt);
}

template <> GstElement* refGPtr<GstElement>(GstElement* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template <> void derefGPtr<GstElement>(GstElement* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}


template <> GRefPtr<GstPlugin> adoptGRef(GstPlugin* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(ptr));
    return GRefPtr<GstPlugin>(ptr, GRefPtrAdopt);
}

template <> GstPlugin* refGPtr<GstPlugin>(GstPlugin* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template <> void derefGPtr<GstPlugin>(GstPlugin* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}

template <> GRefPtr<GstPad> adoptGRef(GstPad* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(ptr));
    return GRefPtr<GstPad>(ptr, GRefPtrAdopt);
}

template <> GstPad* refGPtr<GstPad>(GstPad* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template <> void derefGPtr<GstPad>(GstPad* ptr)
{
    if (ptr)
        gst_object_unref(GST_OBJECT(ptr));
}

template <> GRefPtr<GstPadTemplate> adoptGRef(GstPadTemplate* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(ptr));
    return GRefPtr<GstPadTemplate>(ptr, GRefPtrAdopt);
}

template <> GstPadTemplate* refGPtr<GstPadTemplate>(GstPadTemplate* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template <> void derefGPtr<GstPadTemplate>(GstPadTemplate* ptr)
{
    if (ptr)
        gst_object_unref(GST_OBJECT(ptr));
}

template <> GRefPtr<GstCaps> adoptGRef(GstCaps* ptr)
{
    return GRefPtr<GstCaps>(ptr, GRefPtrAdopt);
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

template <> GRefPtr<GstContext> adoptGRef(GstContext* ptr)
{
    return GRefPtr<GstContext>(ptr, GRefPtrAdopt);
}

template <> GstContext* refGPtr<GstContext>(GstContext* ptr)
{
    if (ptr)
        gst_context_ref(ptr);
    return ptr;
}

template <> void derefGPtr<GstContext>(GstContext* ptr)
{
    if (ptr)
        gst_context_unref(ptr);
}

template <> GRefPtr<GstTask> adoptGRef(GstTask* ptr)
{
    // There is no need to check the object reference is floating here because
    // gst_task_init() always sinks it.
    return GRefPtr<GstTask>(ptr, GRefPtrAdopt);
}

template <> GstTask* refGPtr<GstTask>(GstTask* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template <> void derefGPtr<GstTask>(GstTask* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}

template <> GRefPtr<GstBus> adoptGRef(GstBus* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(ptr));
    return GRefPtr<GstBus>(ptr, GRefPtrAdopt);
}

template <> GstBus* refGPtr<GstBus>(GstBus* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template <> void derefGPtr<GstBus>(GstBus* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}

template <> GRefPtr<GstElementFactory> adoptGRef(GstElementFactory* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(ptr));
    return GRefPtr<GstElementFactory>(ptr, GRefPtrAdopt);
}

template <> GstElementFactory* refGPtr<GstElementFactory>(GstElementFactory* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template <> void derefGPtr<GstElementFactory>(GstElementFactory* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}

template<> GRefPtr<GstBuffer> adoptGRef(GstBuffer* ptr)
{
    return GRefPtr<GstBuffer>(ptr, GRefPtrAdopt);
}

template<> GstBuffer* refGPtr<GstBuffer>(GstBuffer* ptr)
{
    if (ptr)
        gst_buffer_ref(ptr);

    return ptr;
}

template<> void derefGPtr<GstBuffer>(GstBuffer* ptr)
{
    if (ptr)
        gst_buffer_unref(ptr);
}

template<> GRefPtr<GstBufferList> adoptGRef(GstBufferList* ptr)
{
    return GRefPtr<GstBufferList>(ptr, GRefPtrAdopt);
}

template<> GstBufferList* refGPtr<GstBufferList>(GstBufferList* ptr)
{
    if (ptr)
        gst_buffer_list_ref(ptr);

    return ptr;
}

template<> void derefGPtr<GstBufferList>(GstBufferList* ptr)
{
    if (ptr)
        gst_buffer_list_unref(ptr);
}

template<> GRefPtr<GstBufferPool> adoptGRef(GstBufferPool* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(ptr));
    return GRefPtr<GstBufferPool>(ptr, GRefPtrAdopt);
}

template<> GstBufferPool* refGPtr<GstBufferPool>(GstBufferPool* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template<> void derefGPtr<GstBufferPool>(GstBufferPool* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}

template<> GRefPtr<GstSample> adoptGRef(GstSample* ptr)
{
    return GRefPtr<GstSample>(ptr, GRefPtrAdopt);
}

template<> GstSample* refGPtr<GstSample>(GstSample* ptr)
{
    if (ptr)
        gst_sample_ref(ptr);

    return ptr;
}

template<> void derefGPtr<GstSample>(GstSample* ptr)
{
    if (ptr)
        gst_sample_unref(ptr);
}

template<> GRefPtr<GstTagList> adoptGRef(GstTagList* ptr)
{
    return GRefPtr<GstTagList>(ptr, GRefPtrAdopt);
}

template<> GstTagList* refGPtr<GstTagList>(GstTagList* ptr)
{
    if (ptr)
        gst_tag_list_ref(ptr);

    return ptr;
}

template<> void derefGPtr<GstTagList>(GstTagList* ptr)
{
    if (ptr)
        gst_tag_list_unref(ptr);
}

template<> GRefPtr<GstEvent> adoptGRef(GstEvent* ptr)
{
    return GRefPtr<GstEvent>(ptr, GRefPtrAdopt);
}

template<> GstEvent* refGPtr<GstEvent>(GstEvent* ptr)
{
    if (ptr)
        gst_event_ref(ptr);

    return ptr;
}

template<> void derefGPtr<GstEvent>(GstEvent* ptr)
{
    if (ptr)
        gst_event_unref(ptr);
}

template<> GRefPtr<GstToc> adoptGRef(GstToc* ptr)
{
    return GRefPtr<GstToc>(ptr, GRefPtrAdopt);
}

template<> GstToc* refGPtr<GstToc>(GstToc* ptr)
{
    if (ptr)
        return gst_toc_ref(ptr);

    return ptr;
}

template<> void derefGPtr<GstToc>(GstToc* ptr)
{
    if (ptr)
        gst_toc_unref(ptr);
}

template<> GRefPtr<GstMessage> adoptGRef(GstMessage* ptr)
{
    return GRefPtr<GstMessage>(ptr, GRefPtrAdopt);
}

template<> GstMessage* refGPtr<GstMessage>(GstMessage* ptr)
{
    if (ptr)
        return gst_message_ref(ptr);

    return ptr;
}

template<> void derefGPtr<GstMessage>(GstMessage* ptr)
{
    if (ptr)
        gst_message_unref(ptr);
}

template <> GRefPtr<GstQuery> adoptGRef(GstQuery* ptr)
{
    return GRefPtr<GstQuery>(ptr, GRefPtrAdopt);
}

template <> GstQuery* refGPtr<GstQuery>(GstQuery* ptr)
{
    if (ptr)
        gst_query_ref(ptr);
    return ptr;
}

template <> void derefGPtr<GstQuery>(GstQuery* ptr)
{
    if (ptr)
        gst_query_unref(ptr);
}

template <> GRefPtr<GstStream> adoptGRef(GstStream* ptr)
{
    return GRefPtr<GstStream>(ptr, GRefPtrAdopt);
}

template <> GstStream* refGPtr<GstStream>(GstStream* ptr)
{
    if (ptr)
        gst_object_ref(GST_OBJECT_CAST(ptr));

    return ptr;
}

template <> void derefGPtr<GstStream>(GstStream* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}

template <> GRefPtr<GstStreamCollection> adoptGRef(GstStreamCollection* ptr)
{
    return GRefPtr<GstStreamCollection>(ptr, GRefPtrAdopt);
}

template <> GstStreamCollection* refGPtr<GstStreamCollection>(GstStreamCollection* ptr)
{
    if (ptr)
        gst_object_ref(GST_OBJECT_CAST(ptr));

    return ptr;
}

template <> void derefGPtr<GstStreamCollection>(GstStreamCollection* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}

template <>
GRefPtr<GstClock> adoptGRef(GstClock* ptr)
{
    return GRefPtr<GstClock>(ptr, GRefPtrAdopt);
}

template <>
GstClock* refGPtr<GstClock>(GstClock* ptr)
{
    if (ptr)
        gst_object_ref(GST_OBJECT_CAST(ptr));

    return ptr;
}

template <>
void derefGPtr<GstClock>(GstClock* ptr)
{
    if (ptr)
        gst_object_unref(ptr);
}

template <> GRefPtr<WebKitVideoSink> adoptGRef(WebKitVideoSink* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(ptr));
    return GRefPtr<WebKitVideoSink>(ptr, GRefPtrAdopt);
}

template <> WebKitVideoSink* refGPtr<WebKitVideoSink>(WebKitVideoSink* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template <> void derefGPtr<WebKitVideoSink>(WebKitVideoSink* ptr)
{
    if (ptr)
        gst_object_unref(GST_OBJECT(ptr));
}

template <> GRefPtr<WebKitWebSrc> adoptGRef(WebKitWebSrc* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(ptr));
    return GRefPtr<WebKitWebSrc>(ptr, GRefPtrAdopt);
}

// This method is only available for WebKitWebSrc and should not be used for any other type.
// This is only to work around a bug in GST where the URI downloader is not taking the ownership of WebKitWebSrc.
// See https://bugs.webkit.org/show_bug.cgi?id=144040.
GRefPtr<WebKitWebSrc> ensureGRef(WebKitWebSrc* ptr)
{
    if (ptr && g_object_is_floating(ptr))
        gst_object_ref_sink(GST_OBJECT(ptr));
    return GRefPtr<WebKitWebSrc>(ptr);
}

template <> WebKitWebSrc* refGPtr<WebKitWebSrc>(WebKitWebSrc* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template <> void derefGPtr<WebKitWebSrc>(WebKitWebSrc* ptr)
{
    if (ptr)
        gst_object_unref(GST_OBJECT(ptr));
}

#if USE(GSTREAMER_GL)

template<> GRefPtr<GstGLDisplay> adoptGRef(GstGLDisplay* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(ptr));
    return GRefPtr<GstGLDisplay>(ptr, GRefPtrAdopt);
}

template<> GstGLDisplay* refGPtr<GstGLDisplay>(GstGLDisplay* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template<> void derefGPtr<GstGLDisplay>(GstGLDisplay* ptr)
{
    if (ptr)
        gst_object_unref(GST_OBJECT(ptr));
}

template<> GRefPtr<GstGLContext> adoptGRef(GstGLContext* ptr)
{
    ASSERT(!ptr || !g_object_is_floating(ptr));
    return GRefPtr<GstGLContext>(ptr, GRefPtrAdopt);
}

template<> GstGLContext* refGPtr<GstGLContext>(GstGLContext* ptr)
{
    if (ptr)
        gst_object_ref_sink(GST_OBJECT(ptr));

    return ptr;
}

template<> void derefGPtr<GstGLContext>(GstGLContext* ptr)
{
    if (ptr)
        gst_object_unref(GST_OBJECT(ptr));
}

template <> GRefPtr<GstEGLImage> adoptGRef(GstEGLImage* ptr)
{
    return GRefPtr<GstEGLImage>(ptr, GRefPtrAdopt);
}

template <> GstEGLImage* refGPtr<GstEGLImage>(GstEGLImage* ptr)
{
    if (ptr)
        gst_egl_image_ref(ptr);
    return ptr;
}

template <> void derefGPtr<GstEGLImage>(GstEGLImage* ptr)
{
    if (ptr)
        gst_egl_image_unref(ptr);
}

#endif // USE(GSTREAMER_GL)

} // namespace WTF

#endif // USE(GSTREAMER)
