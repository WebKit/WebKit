/*
 *  Copyright (C) 2011 Igalia S.L
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

#pragma once

#if USE(GSTREAMER)
#include <gst/gst.h>
#include <gst/pbutils/encoding-profile.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _WebKitVideoSink WebKitVideoSink;
struct WebKitWebSrc;
typedef struct _GstBaseSink GstBaseSink;

#if USE(GSTREAMER_GL)
typedef struct _GstGLDisplay GstGLDisplay;
typedef struct _GstGLContext GstGLContext;
typedef struct _GstEGLImage GstEGLImage;
typedef struct _GstGLColorConvert GstGLColorConvert;
#endif

#if USE(GSTREAMER_WEBRTC)
typedef struct _GstPromise GstPromise;
typedef struct _GstWebRTCDTLSTransport GstWebRTCDTLSTransport;
typedef struct _GstWebRTCDataChannel GstWebRTCDataChannel;
typedef struct _GstWebRTCICETransport GstWebRTCICETransport;
typedef struct _GstWebRTCRTPReceiver GstWebRTCRTPReceiver;
typedef struct _GstWebRTCRTPSender GstWebRTCRTPSender;
typedef struct _GstWebRTCRTPTransceiver GstWebRTCRTPTransceiver;
typedef struct _GstRTPHeaderExtension GstRTPHeaderExtension;
typedef struct _GstWebRTCICE GstWebRTCICE;
typedef struct _GstWebRTCICEStream GstWebRTCICEStream;

#endif // USE(GSTREAMER_WEBRTC)

namespace WTF {

WTF_DEFINE_GREF_TRAITS_INLINE(GstMiniObject, gst_mini_object_ref, gst_mini_object_unref)

WTF_DEFINE_GREF_TRAITS_INLINE(GstPlugin, gst_object_ref_sink, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstElement, gst_object_ref_sink, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstBaseSink, gst_object_ref_sink, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstPad, gst_object_ref_sink, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstPadTemplate, gst_object_ref_sink, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstTask, gst_object_ref_sink, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstBus, gst_object_ref_sink, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstElementFactory, gst_object_ref_sink, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstBufferPool, gst_object_ref_sink, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(WebKitVideoSink, gst_object_ref_sink, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(WebKitWebSrc, gst_object_ref_sink, gst_object_unref, g_object_is_floating)

WTF_DEFINE_GREF_TRAITS_INLINE(GstObject, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstStream, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstStreamCollection, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstClock, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstDeviceMonitor, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstDeviceProvider, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstDevice, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstTracer, gst_object_ref, gst_object_unref, g_object_is_floating)

WTF_DEFINE_GREF_TRAITS_INLINE(GstCaps, gst_caps_ref, gst_caps_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GstContext, gst_context_ref, gst_context_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GstBuffer, gst_buffer_ref, gst_buffer_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GstBufferList, gst_buffer_list_ref, gst_buffer_list_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GstMemory, gst_memory_ref, gst_memory_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GstSample, gst_sample_ref, gst_sample_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GstTagList, gst_tag_list_ref, gst_tag_list_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GstEvent, gst_event_ref, gst_event_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GstMessage, gst_message_ref, gst_message_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GstQuery, gst_query_ref, gst_query_unref)

WTF_DECLARE_GREF_TRAITS(GstEncodingProfile)

#if USE(GSTREAMER_GL)
WTF_DEFINE_GREF_TRAITS_INLINE(GstGLDisplay, gst_object_ref_sink, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstGLContext, gst_object_ref_sink, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstGLColorConvert, gst_object_ref_sink, gst_object_unref, g_object_is_floating)

WTF_DECLARE_GREF_TRAITS(GstEGLImage)
#endif

#if USE(GSTREAMER_WEBRTC)
WTF_DEFINE_GREF_TRAITS_INLINE(GstWebRTCRTPReceiver, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstWebRTCRTPSender, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstWebRTCRTPTransceiver, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstWebRTCDTLSTransport, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstWebRTCICETransport, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstWebRTCICEStream, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstRTPHeaderExtension, gst_object_ref, gst_object_unref, g_object_is_floating)
WTF_DEFINE_GREF_TRAITS_INLINE(GstWebRTCICE, gst_object_ref, gst_object_unref, g_object_is_floating)

WTF_DEFINE_GREF_TRAITS_INLINE(GstPromise, gst_promise_ref, gst_promise_unref)

#endif // USE(GSTREAMER_WEBRTC)

// GstToc needs to be defined manually because gst_toc_ref() causes a warning if return value is not used.
template<> struct GRefPtrDefaultRefDerefTraits<GstToc> {
    static inline GstToc* refIfNotNull(GstToc* ptr)
    {
        if (ptr) [[likely]]
            return gst_toc_ref(ptr);
        return nullptr;
    }
    static inline void derefIfNotNull(GstToc* ptr)
    {
        if (ptr) [[likely]]
            gst_toc_unref(ptr);
    }
    static inline bool isFloating(GstToc*)
    {
        return false;
    }
};

} // namespace WTF

#endif // USE(GSTREAMER)
