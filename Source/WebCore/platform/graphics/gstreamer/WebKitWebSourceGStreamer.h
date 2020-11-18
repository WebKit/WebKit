/*
 *  Copyright (C) 2009,2010 Sebastian Dröge <sebastian.droege@collabora.co.uk>
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

#if ENABLE(VIDEO) && USE(GSTREAMER)

#include <gst/base/gstpushsrc.h>
#include <gst/gst.h>
#include <wtf/Forward.h>

namespace WebCore {
class MediaPlayer;
class SecurityOrigin;
}

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_SRC            (webkit_web_src_get_type ())
#define WEBKIT_WEB_SRC(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_TYPE_WEB_SRC, WebKitWebSrc))
#define WEBKIT_WEB_SRC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_WEB_SRC, WebKitWebSrcClass))
#define WEBKIT_IS_WEB_SRC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_TYPE_WEB_SRC))
#define WEBKIT_IS_WEB_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), WEBKIT_TYPE_WEB_SRC))
#define WEBKIT_WEB_SRC_CAST(obj)       ((WebKitWebSrc*)(obj))

#define WEBKIT_WEB_SRC_PLAYER_CONTEXT_TYPE_NAME  "webkit.media-player"

struct WebKitWebSrcPrivate;

struct WebKitWebSrc {
    GstPushSrc parent;

    WebKitWebSrcPrivate *priv;
};

struct WebKitWebSrcClass {
    GstPushSrcClass parentClass;
};

GType webkit_web_src_get_type(void);
void webKitWebSrcSetMediaPlayer(WebKitWebSrc*, WebCore::MediaPlayer*, const WTF::String&);
bool webKitSrcPassedCORSAccessCheck(WebKitWebSrc*);
bool webKitSrcWouldTaintOrigin(WebKitWebSrc*, const WebCore::SecurityOrigin&);

G_END_DECLS

#endif // ENABLE(VIDEO) && USE(GSTREAMER)
