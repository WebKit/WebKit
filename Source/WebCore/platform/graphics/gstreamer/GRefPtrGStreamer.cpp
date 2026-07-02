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
#if USE(GSTREAMER_GL)
#include <gst/gl/egl/gsteglimage.h>
#endif

#if USE(GSTREAMER_WEBRTC)
#include <gst/rtp/rtp.h>
#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#undef GST_USE_UNSTABLE_API
#endif

namespace WTF {

WTF_DEFINE_GREF_TRAITS(GstEncodingProfile, gst_encoding_profile_ref, gst_encoding_profile_unref)

#if USE(GSTREAMER_GL)
WTF_DEFINE_GREF_TRAITS(GstEGLImage, gst_egl_image_ref, gst_egl_image_unref)
#endif // USE(GSTREAMER_GL)

} // namespace WTF

#endif // USE(GSTREAMER)
