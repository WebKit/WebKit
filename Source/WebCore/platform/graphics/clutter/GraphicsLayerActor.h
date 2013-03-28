/*
 * Copyright 2011, 2012 Collabora Limited
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef GraphicsLayerActor_h 
#define GraphicsLayerActor_h 

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsLayerClutter.h"
#include <clutter/clutter.h>
#include <cogl/cogl.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GRAPHICS_LAYER_TYPE_ACTOR graphics_layer_actor_get_type()

#define GRAPHICS_LAYER_ACTOR(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), \
    GRAPHICS_LAYER_TYPE_ACTOR, GraphicsLayerActor))

#define GRAPHICS_LAYER_ACTOR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), \
    GRAPHICS_LAYER_TYPE_ACTOR, GraphicsLayerActorClass))

#define GRAPHICS_LAYER_IS_ACTOR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
    GRAPHICS_LAYER_TYPE_ACTOR))

#define GRAPHICS_LAYER_IS_ACTOR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), \
    GRAPHICS_LAYER_TYPE_ACTOR))

#define GRAPHICS_LAYER_ACTOR_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), \
    GRAPHICS_LAYER_TYPE_ACTOR, GraphicsLayerActorClass))

typedef struct _GraphicsLayerActor GraphicsLayerActor;
typedef struct _GraphicsLayerActorClass GraphicsLayerActorClass;
typedef struct _GraphicsLayerActorPrivate GraphicsLayerActorPrivate;

/**
 * GraphicsLayerActor:
 *
 * The contents of this structure are private and should only be accessed
 * through the public API.
 */
struct _GraphicsLayerActor {
    ClutterRectangle parent;
    GraphicsLayerActorPrivate *priv;
    GList *children;
};

struct _GraphicsLayerActorClass {
    ClutterRectangleClass parent_class;
};

GType graphics_layer_actor_get_type(void) G_GNUC_CONST;

GraphicsLayerActor* graphicsLayerActorNew(WebCore::GraphicsLayerClutter::LayerType);
GraphicsLayerActor* graphicsLayerActorNewWithClient(WebCore::GraphicsLayerClutter::LayerType, WebCore::PlatformClutterLayerClient*);
void graphicsLayerActorSetClient(GraphicsLayerActor*, WebCore::PlatformClutterLayerClient*);
WebCore::PlatformClutterLayerClient* graphicsLayerActorGetClient(GraphicsLayerActor*);
void graphicsLayerActorRemoveAll(GraphicsLayerActor*);
cairo_surface_t* graphicsLayerActorGetSurface(GraphicsLayerActor*);
void graphicsLayerActorSetSurface(GraphicsLayerActor*, cairo_surface_t*);
void graphicsLayerActorInvalidateRectangle(GraphicsLayerActor*, const WebCore::FloatRect&);
void graphicsLayerActorSetAnchorPoint(GraphicsLayerActor*, float, float, float);
void graphicsLayerActorGetAnchorPoint(GraphicsLayerActor*, float*, float*, float*);
void graphicsLayerActorSetScrollPosition(GraphicsLayerActor*, float, float); 
void graphicsLayerActorSetTranslateX(GraphicsLayerActor*, float);
float graphicsLayerActorGetTranslateX(GraphicsLayerActor*);
void graphicsLayerActorSetTranslateY(GraphicsLayerActor*, float);
float graphicsLayerActorGetTranslateY(GraphicsLayerActor*);
gint graphicsLayerActorGetnChildren(GraphicsLayerActor*);
WebCore::GraphicsLayerClutter::LayerType graphicsLayerActorGetLayerType(GraphicsLayerActor*);
void graphicsLayerActorSetLayerType(GraphicsLayerActor*, WebCore::GraphicsLayerClutter::LayerType);
void graphicsLayerActorSetSublayers(GraphicsLayerActor*, WebCore::GraphicsLayerActorList&);
gboolean graphicsLayerActorGetDrawsContent(GraphicsLayerActor*);
void graphicsLayerActorSetDrawsContent(GraphicsLayerActor*, gboolean drawsContent);

WebCore::PlatformClutterAnimation* graphicsLayerActorGetAnimationForKey(GraphicsLayerActor*, const String);

G_END_DECLS

#endif // USE(ACCELERATED_COMPOSITING)

#endif /* GraphicsLayerActor_h */
