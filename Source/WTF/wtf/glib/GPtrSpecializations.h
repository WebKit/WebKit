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
// No library headers are included: the macros forward-declare the type as
// typedef struct _Name Name, which is legal even when GTypedefs.h or a library
// header declares the same typedef, and costs nothing when the library is not
// used. The _FOR_TYPE variants are for types whose tag does not follow that
// convention.
//
// GRefPtr<T> and GUniquePtr<T> static_assert that a specialization they see is
// listed here, so a hand-written one cannot be left out either.

namespace WTF {

template<typename> struct GRefPtrDefaultRefDerefTraits;
template<typename> struct GPtrDeleter;

template<typename> inline constexpr bool hasDeclaredGRefTraits = false;
template<typename> inline constexpr bool hasDeclaredGPtrDeleter = false;

} // namespace WTF

#define WTF_DECLARE_GREF_TRAITS_SPECIALIZATION_FOR_TYPE(type) \
    template<> struct WTF::GRefPtrDefaultRefDerefTraits<type>; \
    template<> inline constexpr bool WTF::hasDeclaredGRefTraits<type> = true;

#define WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(name) \
    typedef struct _##name name; \
    WTF_DECLARE_GREF_TRAITS_SPECIALIZATION_FOR_TYPE(name)

#define WTF_DECLARE_GPTR_DELETER_SPECIALIZATION_FOR_TYPE(type) \
    template<> struct WTF::GPtrDeleter<type>; \
    template<> inline constexpr bool WTF::hasDeclaredGPtrDeleter<type> = true;

#define WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(name) \
    typedef struct _##name name; \
    WTF_DECLARE_GPTR_DELETER_SPECIALIZATION_FOR_TYPE(name)

// GLib (GRefPtr.h, GUniquePtr.h)
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
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION_FOR_TYPE(char*) // GStrv

// GStreamer (GRefPtrGStreamer.h, GUniquePtrGStreamer.h)
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
struct WebKitMediaSrcPad;
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION_FOR_TYPE(WebKitMediaSrcPad)
struct WebKitWebSrc;
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION_FOR_TYPE(WebKitWebSrc)

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

// GTK (GRefPtrGtk.h, GUniquePtrGtk.h). GdkEvent is a union on GTK3 and is declared in GTypedefs.h.
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION_FOR_TYPE(GdkEvent)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GskRenderNode)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GtkTargetList)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(GtkWidgetPath)

WTF_DECLARE_GPTR_DELETER_SPECIALIZATION_FOR_TYPE(GdkEvent)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(GtkTreePath)

// WPE Platform (GRefPtrWPE.h)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WPEClipboardContent)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WPEDRMDevice)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WPEEvent)

// WebKit API (WebKitFrameInfoPrivate.h, WebKitImagePrivate.h, WebKitWebViewBackendPrivate.h)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WebKitFrameInfo)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WebKitImageList)
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WebKitUserContentFilter) // TestWebKitUserContentFilterStore.cpp
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(WebKitWebViewBackend)

// libsoup (GUniquePtrSoup.h)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(SoupCookie)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(SoupHSTSPolicy)
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(SoupMessageHeaders)

// Others
WTF_DECLARE_GREF_TRAITS_SPECIALIZATION(SecretValue) // NetworkStorageSessionSoup.cpp
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION(ManetteMonitorIter) // GUniquePtrManette.h
typedef struct cst_voice_struct cst_voice; // GUniquePtrFlite.h
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION_FOR_TYPE(cst_voice)
typedef struct cst_wave_struct cst_wave; // GUniquePtrFlite.h
WTF_DECLARE_GPTR_DELETER_SPECIALIZATION_FOR_TYPE(cst_wave)

#undef WTF_DECLARE_GREF_TRAITS_SPECIALIZATION
#undef WTF_DECLARE_GREF_TRAITS_SPECIALIZATION_FOR_TYPE
#undef WTF_DECLARE_GPTR_DELETER_SPECIALIZATION
#undef WTF_DECLARE_GPTR_DELETER_SPECIALIZATION_FOR_TYPE

#endif // USE(GLIB)
