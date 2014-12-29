/*
 *  Copyright (C) 2009, 2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
 *  Copyright (C) 2013 Collabora Ltd.
 *  Copyright (C) 2013 Orange
 *  Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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

#ifndef WebKitMediaSourceGStreamer_h
#define WebKitMediaSourceGStreamer_h
#if ENABLE(VIDEO) && ENABLE(MEDIA_SOURCE) && USE(GSTREAMER)

#include "GRefPtrGStreamer.h"
#include "MediaPlayer.h"
#include "MediaSource.h"
#include "SourceBufferPrivate.h"
#include "SourceBufferPrivateClient.h"

#include <gst/gst.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_MEDIA_SRC            (webkit_media_src_get_type ())
#define WEBKIT_MEDIA_SRC(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_TYPE_MEDIA_SRC, WebKitMediaSrc))
#define WEBKIT_MEDIA_SRC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_MEDIA_SRC, WebKitMediaSrcClass))
#define WEBKIT_IS_MEDIA_SRC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_TYPE_MEDIA_SRC))
#define WEBKIT_IS_MEDIA_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), WEBKIT_TYPE_MEDIA_SRC))

typedef struct _WebKitMediaSrc        WebKitMediaSrc;
typedef struct _WebKitMediaSrcClass   WebKitMediaSrcClass;
typedef struct _WebKitMediaSrcPrivate WebKitMediaSrcPrivate;

struct _WebKitMediaSrc {
    GstBin parent;

    WebKitMediaSrcPrivate *priv;
};

struct _WebKitMediaSrcClass {
    GstBinClass parentClass;
};

GType webkit_media_src_get_type(void);

G_END_DECLS

namespace WTF {
template<> GRefPtr<WebKitMediaSrc> adoptGRef(WebKitMediaSrc* ptr);
template<> WebKitMediaSrc* refGPtr<WebKitMediaSrc>(WebKitMediaSrc* ptr);
template<> void derefGPtr<WebKitMediaSrc>(WebKitMediaSrc* ptr);
};

namespace WebCore {

class ContentType;

class MediaSourceClientGStreamer: public RefCounted<MediaSourceClientGStreamer> {
    public:
        MediaSourceClientGStreamer(WebKitMediaSrc*);
        virtual ~MediaSourceClientGStreamer();

        // From MediaSourceGStreamer
        MediaSourcePrivate::AddStatus addSourceBuffer(PassRefPtr<SourceBufferPrivate>, const ContentType&);
        void durationChanged(const MediaTime&);
        void markEndOfStream(MediaSourcePrivate::EndOfStreamStatus);

        // From SourceBufferPrivateGStreamer
        SourceBufferPrivateClient::AppendResult append(PassRefPtr<SourceBufferPrivate>, const unsigned char*, unsigned);
        void removedFromMediaSource(PassRefPtr<SourceBufferPrivate>);

    private:
        GRefPtr<WebKitMediaSrc> m_src;
};

};

#endif // USE(GSTREAMER)
#endif
