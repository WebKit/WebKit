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

#include <wtf/glib/GRefPtr.h>

#include <gst/gst.h>
#include <gst/pbutils/encoding-profile.h>

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
#endif

#if USE(GSTREAMER_TRANSCODER)
typedef struct _GstTranscoder GstTranscoder;
typedef struct _GstTranscoderSignalAdapter GstTranscoderSignalAdapter;
#endif

namespace WTF {

template<> GRefPtr<GstPlugin> adoptGRef(GstPlugin* ptr);
template<> GstPlugin* refGPtr<GstPlugin>(GstPlugin* ptr);
template<> void derefGPtr<GstPlugin>(GstPlugin* ptr);

template<> GRefPtr<GstMiniObject> adoptGRef(GstMiniObject* ptr);
template<> GstMiniObject* refGPtr<GstMiniObject>(GstMiniObject* ptr);
template<> void derefGPtr<GstMiniObject>(GstMiniObject* ptr);

template<> GRefPtr<GstElement> adoptGRef(GstElement* ptr);
template<> GstElement* refGPtr<GstElement>(GstElement* ptr);
template<> void derefGPtr<GstElement>(GstElement* ptr);

template<> GRefPtr<GstBaseSink> adoptGRef(GstBaseSink* ptr);
template<> GstBaseSink* refGPtr<GstBaseSink>(GstBaseSink* ptr);
template<> void derefGPtr<GstBaseSink>(GstBaseSink* ptr);

template<> GRefPtr<GstPad> adoptGRef(GstPad* ptr);
template<> GstPad* refGPtr<GstPad>(GstPad* ptr);
template<> void derefGPtr<GstPad>(GstPad* ptr);

template<> GRefPtr<GstPadTemplate> adoptGRef(GstPadTemplate* ptr);
template<> GstPadTemplate* refGPtr<GstPadTemplate>(GstPadTemplate* ptr);
template<> void derefGPtr<GstPadTemplate>(GstPadTemplate* ptr);

template<> GRefPtr<GstCaps> adoptGRef(GstCaps* ptr);
template<> GstCaps* refGPtr<GstCaps>(GstCaps* ptr);
template<> void derefGPtr<GstCaps>(GstCaps* ptr);

template<> GRefPtr<GstContext> adoptGRef(GstContext* ptr);
template<> GstContext* refGPtr<GstContext>(GstContext* ptr);
template<> void derefGPtr<GstContext>(GstContext* ptr);

template<> GRefPtr<GstTask> adoptGRef(GstTask* ptr);
template<> GstTask* refGPtr<GstTask>(GstTask* ptr);
template<> void derefGPtr<GstTask>(GstTask* ptr);

template<> GRefPtr<GstBus> adoptGRef(GstBus* ptr);
template<> GstBus* refGPtr<GstBus>(GstBus* ptr);
template<> void derefGPtr<GstBus>(GstBus* ptr);

template<> GRefPtr<GstElementFactory> adoptGRef(GstElementFactory* ptr);
template<> GstElementFactory* refGPtr<GstElementFactory>(GstElementFactory* ptr);
template<> void derefGPtr<GstElementFactory>(GstElementFactory* ptr);

template<> GRefPtr<GstBuffer> adoptGRef(GstBuffer* ptr);
template<> GstBuffer* refGPtr<GstBuffer>(GstBuffer* ptr);
template<> WEBCORE_EXPORT void derefGPtr<GstBuffer>(GstBuffer* ptr);

template<> GRefPtr<GstBufferList> adoptGRef(GstBufferList*);
template<> GstBufferList* refGPtr<GstBufferList>(GstBufferList*);
template<> void derefGPtr<GstBufferList>(GstBufferList*);

template<> GRefPtr<GstBufferPool> adoptGRef(GstBufferPool*);
template<> GstBufferPool* refGPtr<GstBufferPool>(GstBufferPool*);
template<> void derefGPtr<GstBufferPool>(GstBufferPool*);

template<> GRefPtr<GstMemory> adoptGRef(GstMemory*);
template<> GstMemory* refGPtr<GstMemory>(GstMemory*);
template<> void derefGPtr<GstMemory>(GstMemory*);

template<> GRefPtr<GstSample> adoptGRef(GstSample* ptr);
template<> GstSample* refGPtr<GstSample>(GstSample* ptr);
template<> void derefGPtr<GstSample>(GstSample* ptr);

template<> GRefPtr<GstTagList> adoptGRef(GstTagList* ptr);
template<> GstTagList* refGPtr<GstTagList>(GstTagList* ptr);
template<> void derefGPtr<GstTagList>(GstTagList* ptr);

template<> GRefPtr<GstEvent> adoptGRef(GstEvent* ptr);
template<> GstEvent* refGPtr<GstEvent>(GstEvent* ptr);
template<> void derefGPtr<GstEvent>(GstEvent* ptr);

template<> GRefPtr<GstToc> adoptGRef(GstToc* ptr);
template<> GstToc* refGPtr<GstToc>(GstToc* ptr);
template<> void derefGPtr<GstToc>(GstToc* ptr);

template<> GRefPtr<GstMessage> adoptGRef(GstMessage*);
template<> GstMessage* refGPtr<GstMessage>(GstMessage*);
template<> void derefGPtr<GstMessage>(GstMessage*);

template<> GRefPtr<GstQuery> adoptGRef(GstQuery* ptr);
template<> GstQuery* refGPtr<GstQuery>(GstQuery* ptr);
template<> void derefGPtr<GstQuery>(GstQuery* ptr);

template<> GRefPtr<WebKitVideoSink> adoptGRef(WebKitVideoSink* ptr);
template<> WebKitVideoSink* refGPtr<WebKitVideoSink>(WebKitVideoSink* ptr);
template<> void derefGPtr<WebKitVideoSink>(WebKitVideoSink* ptr);

template<> GRefPtr<WebKitWebSrc> adoptGRef(WebKitWebSrc* ptr);
GRefPtr<WebKitWebSrc> ensureGRef(WebKitWebSrc* ptr);
template<> WebKitWebSrc* refGPtr<WebKitWebSrc>(WebKitWebSrc* ptr);
template<> void derefGPtr<WebKitWebSrc>(WebKitWebSrc* ptr);

template<> GRefPtr<GstStream> adoptGRef(GstStream*);
template<> GstStream* refGPtr<GstStream>(GstStream*);
template<> void derefGPtr<GstStream>(GstStream*);

template<> GRefPtr<GstStreamCollection> adoptGRef(GstStreamCollection*);
template<> GstStreamCollection* refGPtr<GstStreamCollection>(GstStreamCollection*);
template<> void derefGPtr<GstStreamCollection>(GstStreamCollection*);

template<> GRefPtr<GstClock> adoptGRef(GstClock*);
template<> GstClock* refGPtr<GstClock>(GstClock*);
template<> void derefGPtr<GstClock>(GstClock*);

template<> GRefPtr<GstDeviceMonitor> adoptGRef(GstDeviceMonitor*);
template<> GstDeviceMonitor* refGPtr<GstDeviceMonitor>(GstDeviceMonitor*);
template<> void derefGPtr<GstDeviceMonitor>(GstDeviceMonitor*);

template<> GRefPtr<GstDevice> adoptGRef(GstDevice*);
template<> GstDevice* refGPtr<GstDevice>(GstDevice*);
template<> void derefGPtr<GstDevice>(GstDevice*);

#if USE(GSTREAMER_GL)
template<> GRefPtr<GstGLDisplay> adoptGRef(GstGLDisplay* ptr);
template<> GstGLDisplay* refGPtr<GstGLDisplay>(GstGLDisplay* ptr);
template<> void derefGPtr<GstGLDisplay>(GstGLDisplay* ptr);

template<> GRefPtr<GstGLContext> adoptGRef(GstGLContext* ptr);
template<> GstGLContext* refGPtr<GstGLContext>(GstGLContext* ptr);
template<> void derefGPtr<GstGLContext>(GstGLContext* ptr);

template<> GRefPtr<GstEGLImage> adoptGRef(GstEGLImage* ptr);
template<> GstEGLImage* refGPtr<GstEGLImage>(GstEGLImage* ptr);
template<> void derefGPtr<GstEGLImage>(GstEGLImage* ptr);

template<> GRefPtr<GstGLColorConvert> adoptGRef(GstGLColorConvert* ptr);
template<> GstGLColorConvert* refGPtr<GstGLColorConvert>(GstGLColorConvert* ptr);
template<> void derefGPtr<GstGLColorConvert>(GstGLColorConvert* ptr);

#endif

template<> GRefPtr<GstEncodingProfile> adoptGRef(GstEncodingProfile*);
template<> GstEncodingProfile* refGPtr<GstEncodingProfile>(GstEncodingProfile*);
template<> void derefGPtr<GstEncodingProfile>(GstEncodingProfile*);

#if USE(GSTREAMER_WEBRTC)
template <> GRefPtr<GstWebRTCRTPReceiver> adoptGRef(GstWebRTCRTPReceiver*);
template <> GstWebRTCRTPReceiver* refGPtr<GstWebRTCRTPReceiver>(GstWebRTCRTPReceiver*);
template <> void derefGPtr<GstWebRTCRTPReceiver>(GstWebRTCRTPReceiver*);

template <> GRefPtr<GstWebRTCRTPSender> adoptGRef(GstWebRTCRTPSender*);
template <> GstWebRTCRTPSender* refGPtr<GstWebRTCRTPSender>(GstWebRTCRTPSender*);
template <> void derefGPtr<GstWebRTCRTPSender>(GstWebRTCRTPSender*);

template <> GRefPtr<GstWebRTCRTPTransceiver> adoptGRef(GstWebRTCRTPTransceiver*);
template <> GstWebRTCRTPTransceiver* refGPtr<GstWebRTCRTPTransceiver>(GstWebRTCRTPTransceiver*);
template <> void derefGPtr<GstWebRTCRTPTransceiver>(GstWebRTCRTPTransceiver*);

template <> GRefPtr<GstWebRTCDataChannel> adoptGRef(GstWebRTCDataChannel*);
template <> GstWebRTCDataChannel* refGPtr<GstWebRTCDataChannel>(GstWebRTCDataChannel*);
template <> void derefGPtr<GstWebRTCDataChannel>(GstWebRTCDataChannel*);

template <> GRefPtr<GstWebRTCDTLSTransport> adoptGRef(GstWebRTCDTLSTransport*);
template <> GstWebRTCDTLSTransport* refGPtr<GstWebRTCDTLSTransport>(GstWebRTCDTLSTransport*);
template <> void derefGPtr<GstWebRTCDTLSTransport>(GstWebRTCDTLSTransport*);

template <> GRefPtr<GstWebRTCICETransport> adoptGRef(GstWebRTCICETransport*);
template <> GstWebRTCICETransport* refGPtr<GstWebRTCICETransport>(GstWebRTCICETransport*);
template <> void derefGPtr<GstWebRTCICETransport>(GstWebRTCICETransport*);

template <> GRefPtr<GstPromise> adoptGRef(GstPromise*);
template <> GstPromise* refGPtr<GstPromise>(GstPromise*);
template <> void derefGPtr<GstPromise>(GstPromise*);

template<> GRefPtr<GstRTPHeaderExtension> adoptGRef(GstRTPHeaderExtension*);
template<> GstRTPHeaderExtension* refGPtr<GstRTPHeaderExtension>(GstRTPHeaderExtension*);
template<> void derefGPtr<GstRTPHeaderExtension>(GstRTPHeaderExtension*);

template<> GRefPtr<GstWebRTCICE> adoptGRef(GstWebRTCICE*);
template<> GstWebRTCICE* refGPtr<GstWebRTCICE>(GstWebRTCICE*);
template<> void derefGPtr<GstWebRTCICE>(GstWebRTCICE*);

#endif

#if USE(GSTREAMER_TRANSCODER)
template<> GRefPtr<GstTranscoder> adoptGRef(GstTranscoder*);
template<> GstTranscoder* refGPtr<GstTranscoder>(GstTranscoder*);
template<> void derefGPtr<GstTranscoder>(GstTranscoder*);

template<> GRefPtr<GstTranscoderSignalAdapter> adoptGRef(GstTranscoderSignalAdapter*);
template<> GstTranscoderSignalAdapter* refGPtr<GstTranscoderSignalAdapter>(GstTranscoderSignalAdapter*);
template<> void derefGPtr<GstTranscoderSignalAdapter>(GstTranscoderSignalAdapter*);
#endif // USE(GSTREAMER_TRANSCODER)

} // namespace WTF

#endif // USE(GSTREAMER)
