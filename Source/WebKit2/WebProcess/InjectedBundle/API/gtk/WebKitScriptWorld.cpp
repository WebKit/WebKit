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
#include "WebKitScriptWorld.h"

#include "WebKitMarshal.h"
#include "WebKitPrivate.h"
#include "WebKitScriptWorldPrivate.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

using namespace WebKit;
using namespace WebCore;

enum {
    WINDOW_OBJECT_CLEARED,

    LAST_SIGNAL
};

typedef HashMap<InjectedBundleScriptWorld*, WebKitScriptWorld*> ScriptWorldMap;

static ScriptWorldMap& scriptWorlds()
{
    static NeverDestroyed<ScriptWorldMap> map;
    return map;
}

struct _WebKitScriptWorldPrivate {
    ~_WebKitScriptWorldPrivate()
    {
        ASSERT(scriptWorlds().contains(scriptWorld.get()));
        scriptWorlds().remove(scriptWorld.get());
    }

    RefPtr<InjectedBundleScriptWorld> scriptWorld;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitScriptWorld, webkit_script_world, G_TYPE_OBJECT)

static void webkit_script_world_class_init(WebKitScriptWorldClass* klass)
{
    /**
     * WebKitScriptWorld::window-object-cleared:
     * @world: the #WebKitScriptWorld on which the signal is emitted
     * @page: a #WebKitWebPage
     * @frame: the #WebKitFrame  to which @world belongs
     *
     * Emitted when the JavaScript window object in a #WebKitScriptWorld has been
     * cleared. This is the preferred place to set custom properties on the window
     * object using the JavaScriptCore API. You can get the window object of @frame
     * from the JavaScript execution context of @world that is returned by
     * webkit_frame_get_javascript_context_for_script_world().
     *
     * Since: 2.2
     */
    signals[WINDOW_OBJECT_CLEARED] = g_signal_new(
        "window-object-cleared",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, 0, 0,
        webkit_marshal_VOID__OBJECT_OBJECT,
        G_TYPE_NONE, 2,
        WEBKIT_TYPE_WEB_PAGE,
        WEBKIT_TYPE_FRAME);
}

WebKitScriptWorld* webkitScriptWorldGet(InjectedBundleScriptWorld* scriptWorld)
{
    return scriptWorlds().get(scriptWorld);
}

InjectedBundleScriptWorld* webkitScriptWorldGetInjectedBundleScriptWorld(WebKitScriptWorld* world)
{
    return world->priv->scriptWorld.get();
}

void webkitScriptWorldWindowObjectCleared(WebKitScriptWorld* world, WebKitWebPage* page, WebKitFrame* frame)
{
    g_signal_emit(world, signals[WINDOW_OBJECT_CLEARED], 0, page, frame);
}

static WebKitScriptWorld* webkitScriptWorldCreate(PassRefPtr<InjectedBundleScriptWorld> scriptWorld)
{
    WebKitScriptWorld* world = WEBKIT_SCRIPT_WORLD(g_object_new(WEBKIT_TYPE_SCRIPT_WORLD, NULL));
    world->priv->scriptWorld = scriptWorld;

    ASSERT(!scriptWorlds().contains(world->priv->scriptWorld.get()));
    scriptWorlds().add(world->priv->scriptWorld.get(), world);

    return world;
}

static gpointer createDefaultScriptWorld(gpointer)
{
    return webkitScriptWorldCreate(InjectedBundleScriptWorld::normalWorld());
}

/**
 * webkit_script_world_get_default:
 *
 * Get the default #WebKitScriptWorld. This is the normal script world
 * where all scripts are executed by default.
 * You can get the JavaScript execution context of a #WebKitScriptWorld
 * for a given #WebKitFrame with webkit_frame_get_javascript_context_for_script_world().
 *
 * Returns: (transfer none): the default #WebKitScriptWorld
 *
 * Since: 2.2
 */
WebKitScriptWorld* webkit_script_world_get_default(void)
{
    static GOnce onceInit = G_ONCE_INIT;
    return WEBKIT_SCRIPT_WORLD(g_once(&onceInit, createDefaultScriptWorld, 0));
}

/**
 * webkit_script_world_new:
 *
 * Creates a new isolated #WebKitScriptWorld. Scripts executed in
 * isolated worlds have access to the DOM but not to other variable
 * or functions created by the page.
 * You can get the JavaScript execution context of a #WebKitScriptWorld
 * for a given #WebKitFrame with webkit_frame_get_javascript_context_for_script_world().
 *
 * Returns: (transfer full): a new isolated #WebKitScriptWorld
 *
 * Since: 2.2
 */
WebKitScriptWorld* webkit_script_world_new(void)
{
    return webkitScriptWorldCreate(InjectedBundleScriptWorld::create());
}
