/*
 *  Copyright (C) 2011 Igalia S.L.
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
#include "WebKitDOMCustom.h"

#include "JSMainThreadExecState.h"
#include "WebKitDOMAudioTrackPrivate.h"
#include "WebKitDOMHTMLInputElement.h"
#include "WebKitDOMHTMLInputElementPrivate.h"
#include "WebKitDOMHTMLMediaElementPrivate.h"
#include "WebKitDOMHTMLTextAreaElement.h"
#include "WebKitDOMHTMLTextAreaElementPrivate.h"
#include "WebKitDOMTextTrackPrivate.h"
#include "WebKitDOMVideoTrackPrivate.h"
#include "gobject/ConvertToUTF8String.h"

using namespace WebKit;

gboolean webkit_dom_html_text_area_element_is_edited(WebKitDOMHTMLTextAreaElement* area)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(area), FALSE);

    return core(area)->lastChangeWasUserEdit();
}

gboolean webkit_dom_html_input_element_is_edited(WebKitDOMHTMLInputElement* input)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(input), FALSE);

    return core(input)->lastChangeWasUserEdit();
}

void webkit_dom_html_media_element_set_current_time(WebKitDOMHTMLMediaElement* self, gdouble value, GError**)
{
#if ENABLE(VIDEO)
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_HTML_MEDIA_ELEMENT(self));
    WebCore::HTMLMediaElement* item = WebKit::core(self);
    item->setCurrentTime(value);
#else
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Video")
#endif /* ENABLE(VIDEO) */
}

gchar* webkit_dom_audio_track_get_kind(WebKitDOMAudioTrack* self)
{
#if ENABLE(VIDEO_TRACK)
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_AUDIO_TRACK(self), 0);
    WebCore::AudioTrack* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->kind());
    return result;
#else
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Video Track")
    return 0;
#endif /* ENABLE(VIDEO_TRACK) */
}

gchar* webkit_dom_audio_track_get_language(WebKitDOMAudioTrack* self)
{
#if ENABLE(VIDEO_TRACK)
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_AUDIO_TRACK(self), 0);
    WebCore::AudioTrack* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->language());
    return result;
#else
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Video Track")
    return 0;
#endif /* ENABLE(VIDEO_TRACK) */
}

gchar* webkit_dom_text_track_get_kind(WebKitDOMTextTrack* self)
{
#if ENABLE(VIDEO_TRACK)
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_TEXT_TRACK(self), 0);
    WebCore::TextTrack* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->kind());
    return result;
#else
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Video Track")
    return 0;
#endif /* ENABLE(VIDEO_TRACK) */
}

gchar* webkit_dom_text_track_get_language(WebKitDOMTextTrack* self)
{
#if ENABLE(VIDEO_TRACK)
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_TEXT_TRACK(self), 0);
    WebCore::TextTrack* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->language());
    return result;
#else
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Video Track")
    return 0;
#endif /* ENABLE(VIDEO_TRACK) */
}

gchar* webkit_dom_video_track_get_kind(WebKitDOMVideoTrack* self)
{
#if ENABLE(VIDEO_TRACK)
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_VIDEO_TRACK(self), 0);
    WebCore::VideoTrack* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->kind());
    return result;
#else
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Video Track")
    return 0;
#endif /* ENABLE(VIDEO_TRACK) */
}

gchar* webkit_dom_video_track_get_language(WebKitDOMVideoTrack* self)
{
#if ENABLE(VIDEO_TRACK)
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_VIDEO_TRACK(self), 0);
    WebCore::VideoTrack* item = WebKit::core(self);
    gchar* result = convertToUTF8String(item->language());
    return result;
#else
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Video Track")
    return 0;
#endif /* ENABLE(VIDEO_TRACK) */
}

