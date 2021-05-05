/*
 *  Copyright (C) 2009, 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *  Copyright (C) 2013 Collabora Ltd.
 *  Copyright (C) 2013 Orange
 *  Copyright (C) 2014, 2015 Sebastian Dröge <sebastian@centricular.com>
 *  Copyright (C) 2015, 2016, 2018, 2019, 2020, 2021 Metrological Group B.V.
 *  Copyright (C) 2015, 2016, 2018, 2019, 2020, 2021 Igalia, S.L
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

#pragma once

#if ENABLE(VIDEO) && ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "MediaSourcePrivate.h"
#include "SourceBufferPrivate.h"
#include "SourceBufferPrivateClient.h"
#include "SourceBufferPrivateGStreamer.h"

namespace WebCore {

class MediaPlayerPrivateGStreamerMSE;

} // namespace WebCore

G_BEGIN_DECLS

#define WEBKIT_TYPE_MEDIA_SRC            (webkit_media_src_get_type ())
#define WEBKIT_MEDIA_SRC(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_TYPE_MEDIA_SRC, WebKitMediaSrc))
#define WEBKIT_MEDIA_SRC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_MEDIA_SRC, WebKitMediaSrcClass))
#define WEBKIT_IS_MEDIA_SRC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_TYPE_MEDIA_SRC))
#define WEBKIT_IS_MEDIA_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), WEBKIT_TYPE_MEDIA_SRC))

struct WebKitMediaSrcPrivate;

struct WebKitMediaSrc {
    GstElement parent;

    WebKitMediaSrcPrivate *priv;
};

struct WebKitMediaSrcClass {
    GstElementClass parentClass;
};

GType webkit_media_src_get_type(void);

void webKitMediaSrcEmitStreams(WebKitMediaSrc* source, const Vector<RefPtr<WebCore::MediaSourceTrackGStreamer>>& tracks);

void webKitMediaSrcFlush(WebKitMediaSrc*, const AtomString& streamName);

G_END_DECLS

namespace WTF {
template<> GRefPtr<WebKitMediaSrc> adoptGRef(WebKitMediaSrc* ptr);
template<> WebKitMediaSrc* refGPtr<WebKitMediaSrc>(WebKitMediaSrc* ptr);
template<> void derefGPtr<WebKitMediaSrc>(WebKitMediaSrc* ptr);
} // namespace WTF

#endif // USE(GSTREAMER)
