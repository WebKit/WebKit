/*
 *  Copyright (C) 2015 Igalia S.L
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

#include <gst/audio/audio.h>
#include <gst/base/gstbytereader.h>
#include <gst/base/gstflowcombiner.h>
#include <gst/gstsegment.h>
#include <gst/gststructure.h>
#include <gst/pbutils/install-plugins.h>
#include <gst/video/video.h>
#include <wtf/glib/GUniquePtr.h>

#if USE(WPE_VIDEO_PLANE_DISPLAY_DMABUF)
#include <wpe/extensions/video-plane-display-dmabuf.h>
#endif

#if defined(BUILDING_WebCore) && PLATFORM(WPE) && USE(WPEBACKEND_FDO_AUDIO_EXTENSION)
#include <wpe/extensions/audio.h>
#endif

namespace WTF {

WTF_DEFINE_GPTR_DELETER(GstStructure, gst_structure_free)
WTF_DEFINE_GPTR_DELETER(GstInstallPluginsContext, gst_install_plugins_context_free)
WTF_DEFINE_GPTR_DELETER(GstIterator, gst_iterator_free)
WTF_DEFINE_GPTR_DELETER(GstSegment, gst_segment_free)
WTF_DEFINE_GPTR_DELETER(GstFlowCombiner, gst_flow_combiner_free)
WTF_DEFINE_GPTR_DELETER(GstByteReader, gst_byte_reader_free)
WTF_DEFINE_GPTR_DELETER(GstVideoConverter, gst_video_converter_free)
WTF_DEFINE_GPTR_DELETER(GstAudioConverter, gst_audio_converter_free)

#if USE(WPE_VIDEO_PLANE_DISPLAY_DMABUF)
WTF_DEFINE_GPTR_DELETER(struct wpe_video_plane_display_dmabuf_source, wpe_video_plane_display_dmabuf_source_destroy)
#endif

#if defined(BUILDING_WebCore) && PLATFORM(WPE) && USE(WPEBACKEND_FDO_AUDIO_EXTENSION)
WTF_DEFINE_GPTR_DELETER(struct wpe_audio_source, wpe_audio_source_destroy)
#endif
}

#endif // USE(GSTREAMER)

