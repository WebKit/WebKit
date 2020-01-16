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
#include <wtf/glib/GRefPtr.h>

typedef struct _WebKitVideoSink WebKitVideoSink;
typedef struct _WebKitWebSrc WebKitWebSrc;

#if USE(GSTREAMER_GL)
typedef struct _GstGLDisplay GstGLDisplay;
typedef struct _GstGLContext GstGLContext;
typedef struct _GstEGLImage GstEGLImage;
#endif

namespace WTF {

template<> GRefPtr<GstPlugin> adoptGRef(GstPlugin* ptr);
template<> GstPlugin* refGPtr<GstPlugin>(GstPlugin* ptr);
template<> void derefGPtr<GstPlugin>(GstPlugin* ptr);

template<> GRefPtr<GstElement> adoptGRef(GstElement* ptr);
template<> GstElement* refGPtr<GstElement>(GstElement* ptr);
template<> void derefGPtr<GstElement>(GstElement* ptr);

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
template<> void derefGPtr<GstBuffer>(GstBuffer* ptr);

template<> GRefPtr<GstBufferList> adoptGRef(GstBufferList*);
template<> GstBufferList* refGPtr<GstBufferList>(GstBufferList*);
template<> void derefGPtr<GstBufferList>(GstBufferList*);

template<> GRefPtr<GstBufferPool> adoptGRef(GstBufferPool*);
template<> GstBufferPool* refGPtr<GstBufferPool>(GstBufferPool*);
template<> void derefGPtr<GstBufferPool>(GstBufferPool*);

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
#endif

} // namespace WTF

#endif // USE(GSTREAMER)
