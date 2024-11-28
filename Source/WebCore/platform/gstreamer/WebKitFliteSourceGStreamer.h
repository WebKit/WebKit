/*
 *  Copyright (C) 2023 Igalia S.L
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

#if USE(FLITE) && USE(GSTREAMER)

#include <gst/gst.h>
#include <wtf/Forward.h>

namespace WebCore {
class PlatformSpeechSynthesisVoice;
}

#define WEBKIT_TYPE_FLITE_SRC (webkit_flite_src_get_type())
#define WEBKIT_FLITE_SRC(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_FLITE_SRC, WebKitFliteSrc))

typedef struct _WebKitFliteSrc WebKitFliteSrc;

GType webkit_flite_src_get_type();

Vector<Ref<WebCore::PlatformSpeechSynthesisVoice>>& ensureFliteVoicesInitialized();
void webKitFliteSrcSetUtterance(WebKitFliteSrc*, const WebCore::PlatformSpeechSynthesisVoice*, const String&);

#endif // USE(FLITE) && USE(GSTREAMER)
