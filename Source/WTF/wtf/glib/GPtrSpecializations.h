/*
 *  Copyright (C) 2026 Igalia S.L.
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
 *
 */

#pragma once

#if USE(GLIB)

#include <wtf/glib/GTypedefs.h>

// Every explicit specialization of GRefPtrDefaultRefDerefTraits and GPtrDeleter
// is declared here, without being defined, so that every translation unit that
// includes GRefPtr.h or GUniquePtr.h knows the specialization exists. Using
// GRefPtr<T> or GUniquePtr<T> for one of these types without including the
// header that defines the specialization is then a compile error (incomplete
// type) rather than a silent fallback to g_object_unref() or g_free().
//
// Only the library typedefs are repeated here (see also GTypedefs.h): no library headers are included.
// A typedef may be redeclared, and a forward-declared struct tag costs nothing even
// when the library is not used. Types declared as typedef struct { ... } X have
// no tag and cannot be listed; see WTF_DEFINE_GPTR_DELETER_UNCHECKED.
//
// The WTF_DECLARE_GREF_TRAITS, WTF_DEFINE_GREF_TRAITS* and WTF_DEFINE_GPTR_DELETER
// macros static_assert that the type is listed here.

// GLib (the rest are in GTypedefs.h)
typedef struct _GArray GArray;
typedef struct _GDateTime GDateTime;
typedef struct _GDBusNodeInfo GDBusNodeInfo;
typedef struct _GMappedFile GMappedFile;
typedef struct _GMarkupParseContext GMarkupParseContext;
typedef struct _GUri GUri;
typedef struct _GValueArray GValueArray;

// GStreamer (GRefPtrGStreamer.h, GUniquePtrGStreamer.h)
typedef struct _GstAudioConverter GstAudioConverter;
typedef struct _GstAudioInfo GstAudioInfo;
typedef struct _GstBaseSink GstBaseSink;
typedef struct _GstBuffer GstBuffer;
typedef struct _GstBufferList GstBufferList;
typedef struct _GstBufferPool GstBufferPool;
typedef struct _GstBus GstBus;
typedef struct _GstCaps GstCaps;
typedef struct _GstClock GstClock;
typedef struct _GstContext GstContext;
typedef struct _GstDevice GstDevice;
typedef struct _GstDeviceMonitor GstDeviceMonitor;
typedef struct _GstDeviceProvider GstDeviceProvider;
typedef struct _GstDeviceProviderFactory GstDeviceProviderFactory;
typedef struct _GstEGLImage GstEGLImage;
typedef struct _GstElement GstElement;
typedef struct _GstElementFactory GstElementFactory;
typedef struct _GstEncodingProfile GstEncodingProfile;
typedef struct _GstEvent GstEvent;
typedef struct _GstFFTF32 GstFFTF32;
typedef struct _GstFlowCombiner GstFlowCombiner;
typedef struct _GstGLColorConvert GstGLColorConvert;
typedef struct _GstGLContext GstGLContext;
typedef struct _GstGLDisplay GstGLDisplay;
typedef struct _GstInstallPluginsContext GstInstallPluginsContext;
typedef struct _GstIterator GstIterator;
typedef struct _GstMemory GstMemory;
typedef struct _GstMessage GstMessage;
typedef struct _GstMiniObject GstMiniObject;
typedef struct _GstObject GstObject;
typedef struct _GstPad GstPad;
typedef struct _GstPadTemplate GstPadTemplate;
typedef struct _GstPlugin GstPlugin;
typedef struct _GstQuery GstQuery;
typedef struct _GstSample GstSample;
typedef struct _GstSegment GstSegment;
typedef struct _GstStream GstStream;
typedef struct _GstStreamCollection GstStreamCollection;
typedef struct _GstStructure GstStructure;
typedef struct _GstTagList GstTagList;
typedef struct _GstTask GstTask;
typedef struct _GstToc GstToc;
typedef struct _GstTracer GstTracer;
typedef struct _GstVideoConverter GstVideoConverter;
typedef struct _GstVideoInfo GstVideoInfo;
typedef struct _WebKitVideoSink WebKitVideoSink;
struct WebKitMediaSrcPad;
struct WebKitWebSrc;

// GTK (GRefPtrGtk.h, GUniquePtrGtk.h)
typedef struct _GskRenderNode GskRenderNode;
typedef struct _GtkTargetList GtkTargetList;
typedef struct _GtkTreePath GtkTreePath;
typedef struct _GtkWidgetPath GtkWidgetPath;

// WPE Platform (GRefPtrWPE.h)
typedef struct _WPEClipboardContent WPEClipboardContent;
typedef struct _WPEDRMDevice WPEDRMDevice;
typedef struct _WPEEvent WPEEvent;

// WebKit API (WebKitFrameInfoPrivate.h, WebKitImagePrivate.h, WebKitWebViewBackendPrivate.h)
typedef struct _WebKitFrameInfo WebKitFrameInfo;
typedef struct _WebKitImageList WebKitImageList;
typedef struct _WebKitUserContentFilter WebKitUserContentFilter;
typedef struct _WebKitWebViewBackend WebKitWebViewBackend;

// libsoup (GUniquePtrSoup.h)
typedef struct _SoupCookie SoupCookie;
typedef struct _SoupHSTSPolicy SoupHSTSPolicy;
typedef struct _SoupMessageHeaders SoupMessageHeaders;

// Others
typedef struct _ManetteMonitorIter ManetteMonitorIter; // GUniquePtrManette.h
typedef struct _SecretValue SecretValue; // NetworkStorageSessionSoup.cpp
typedef struct cst_voice_struct cst_voice; // GUniquePtrFlite.h

namespace WTF {

template<typename> struct GRefPtrDefaultRefDerefTraits;
template<typename> struct GPtrDeleter;

template<typename> inline constexpr bool hasDeclaredGRefTraits = false;
template<typename> inline constexpr bool hasDeclaredGPtrDeleter = false;

#define WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(typeName) \
    template<> struct GRefPtrDefaultRefDerefTraits<typeName>; \
    template<> inline constexpr bool hasDeclaredGRefTraits<typeName> = true;

#define WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(typeName) \
    template<> struct GPtrDeleter<typeName>; \
    template<> inline constexpr bool hasDeclaredGPtrDeleter<typeName> = true;

// GLib
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GArray)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GByteArray)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GBytes)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GClosure)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GDateTime)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GDBusNodeInfo)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GHashTable)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GKeyFile)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GMainContext)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GMainLoop)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GMappedFile)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GPtrArray)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GResource)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GSource)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GUri)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GVariant)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GVariantBuilder)

WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GDir)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GError)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GKeyFile)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GList)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GMarkupParseContext)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GPatternSpec)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GSList)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GTimer)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GValueArray)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GVariantIter)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GVariantType)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(char*)

// GStreamer
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstBaseSink)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstBuffer)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstBufferList)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstBufferPool)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstBus)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstCaps)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstClock)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstContext)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstDevice)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstDeviceMonitor)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstDeviceProvider)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstDeviceProviderFactory)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstEGLImage)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstElement)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstElementFactory)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstEncodingProfile)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstEvent)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstGLColorConvert)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstGLContext)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstGLDisplay)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstMemory)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstMessage)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstMiniObject)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstObject)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstPad)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstPadTemplate)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstPlugin)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstQuery)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstSample)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstStream)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstStreamCollection)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstTagList)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstTask)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstToc)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GstTracer)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WebKitVideoSink)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WebKitMediaSrcPad)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WebKitWebSrc)

WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GstAudioConverter)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GstAudioInfo)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GstFFTF32)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GstFlowCombiner)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GstInstallPluginsContext)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GstIterator)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GstSegment)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GstStructure)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GstVideoConverter)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GstVideoInfo)

// GTK
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GdkEvent)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GskRenderNode)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GtkTargetList)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GtkWidgetPath)

WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GdkEvent)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GtkTreePath)

// WPE Platform
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WPEClipboardContent)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WPEDRMDevice)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WPEEvent)

// WebKit API
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WebKitFrameInfo)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WebKitImageList)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WebKitUserContentFilter)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WebKitWebViewBackend)

// libsoup
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(SoupCookie)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(SoupHSTSPolicy)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(SoupMessageHeaders)

// Others
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(SecretValue)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(ManetteMonitorIter)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(cst_voice)

#undef WTF_DECLARE_GREF_TRAITS_SPECIALIZATION
#undef WTF_DECLARE_GPTR_DELETER_SPECIALIZATION

#define WTF_CHECK_GREF_TRAITS_DECLARED(typeName) \
    static_assert(hasDeclaredGRefTraits<typeName>, #typeName ": declare its GRefPtr traits in wtf/glib/GPtrSpecializations.h");

#define WTF_CHECK_GPTR_DELETER_DECLARED(typeName) \
    static_assert(hasDeclaredGPtrDeleter<typeName>, #typeName ": declare its GPtrDeleter in wtf/glib/GPtrSpecializations.h");

} // namespace WTF

#endif // USE(GLIB)
