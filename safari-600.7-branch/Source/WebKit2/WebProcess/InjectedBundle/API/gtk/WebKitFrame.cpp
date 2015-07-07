/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitFrame.h"

#include "WebKitFramePrivate.h"
#include "WebKitPrivate.h"
#include "WebKitScriptWorldPrivate.h"
#include <wtf/text/CString.h>

using namespace WebKit;
using namespace WebCore;

struct _WebKitFramePrivate {
    RefPtr<WebFrame> webFrame;

    CString uri;
};

WEBKIT_DEFINE_TYPE(WebKitFrame, webkit_frame, G_TYPE_OBJECT)

static void webkit_frame_class_init(WebKitFrameClass*)
{
}

WebKitFrame* webkitFrameCreate(WebFrame* webFrame)
{
    WebKitFrame* frame = WEBKIT_FRAME(g_object_new(WEBKIT_TYPE_FRAME, NULL));
    frame->priv->webFrame = webFrame;
    return frame;
}

/**
 * webkit_frame_is_main_frame:
 * @frame: a #WebKitFrame
 *
 * Gets whether @frame is the a main frame of a #WebKitWebPage
 *
 * Returns: %TRUE if @frame is a main frame or %FALSE otherwise
 *
 * Since: 2.2
 */
gboolean webkit_frame_is_main_frame(WebKitFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), FALSE);

    return frame->priv->webFrame->isMainFrame();
}

/**
 * webkit_frame_get_uri:
 * @frame: a #WebKitFrame
 *
 * Gets the current active URI of @frame.
 *
 * Returns: the current active URI of @frame or %NULL if nothing has been
 *    loaded yet.
 *
 * Since: 2.2
 */
const gchar* webkit_frame_get_uri(WebKitFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), 0);

    if (frame->priv->uri.isNull())
        frame->priv->uri = frame->priv->webFrame->url().utf8();

    return frame->priv->uri.data();
}

/**
 * webkit_frame_get_javascript_global_context:
 * @frame: a #WebKitFrame
 *
 * Gets the global JavaScript execution context. Use this function to bridge
 * between the WebKit and JavaScriptCore APIs.
 *
 * Returns: (transfer none): the global JavaScript context of @frame
 *
 * Since: 2.2
 */
JSGlobalContextRef webkit_frame_get_javascript_global_context(WebKitFrame* frame)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), 0);

    return frame->priv->webFrame->jsContext();
}

/**
 * webkit_frame_get_javascript_context_for_script_world:
 * @frame: a #WebKitFrame
 * @world: a #WebKitScriptWorld
 *
 * Gets the JavaScript execution context of @frame for the given #WebKitScriptWorld.
 *
 * Returns: (transfer none): the JavaScript context of @frame for @world
 *
 * Since: 2.2
 */
JSGlobalContextRef webkit_frame_get_javascript_context_for_script_world(WebKitFrame* frame, WebKitScriptWorld* world)
{
    g_return_val_if_fail(WEBKIT_IS_FRAME(frame), 0);
    g_return_val_if_fail(WEBKIT_IS_SCRIPT_WORLD(world), 0);

    return frame->priv->webFrame->jsContextForWorld(webkitScriptWorldGetInjectedBundleScriptWorld(world));
}
