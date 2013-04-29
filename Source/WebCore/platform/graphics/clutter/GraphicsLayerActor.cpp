/*
 * Copyright 2011, 2012, 2013 Collabora Limited
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

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsLayerActor.h"

#include "GraphicsContext.h"
#include "GraphicsLayerClutter.h"
#include "PlatformClutterLayerClient.h"
#include "PlatformContextCairo.h"
#include "RefPtrCairo.h"
#include "TransformationMatrix.h"
#include <algorithm>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/text/CString.h>

using namespace WebCore;

G_DEFINE_TYPE(GraphicsLayerActor, graphics_layer_actor, CLUTTER_TYPE_ACTOR)

#define GRAPHICS_LAYER_ACTOR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GRAPHICS_LAYER_TYPE_ACTOR, GraphicsLayerActorPrivate))

struct _GraphicsLayerActorPrivate {
    PlatformClutterLayerClient* layerClient;
    RefPtr<cairo_surface_t> surface;

    GraphicsLayerClutter::LayerType layerType;

    bool flatten;
    bool drawsContent;

    float scrollX;
    float scrollY;

    float translateX;
    float translateY;
};

enum {
    Property0,

    PropertyTranslateX,
    PropertyTranslateY,

    PropertyLast
};

static void graphicsLayerActorAllocate(ClutterActor*, const ClutterActorBox*, ClutterAllocationFlags);
static void graphicsLayerActorApplyTransform(ClutterActor*, CoglMatrix*);
static void graphicsLayerActorDispose(GObject*);
static void graphicsLayerActorGetProperty(GObject*, guint propID, GValue*, GParamSpec*);
static void graphicsLayerActorSetProperty(GObject*, guint propID, const GValue*, GParamSpec*);
static void graphicsLayerActorPaint(ClutterActor*);

static gboolean graphicsLayerActorDraw(ClutterCanvas*, cairo_t*, gint width, gint height, GraphicsLayerActor*);
static void graphicsLayerActorUpdateTexture(GraphicsLayerActor*);
static void drawLayerContents(ClutterActor*, GraphicsContext&);

static void graphics_layer_actor_class_init(GraphicsLayerActorClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    ClutterActorClass* actorClass = CLUTTER_ACTOR_CLASS(klass);

    objectClass->get_property = graphicsLayerActorGetProperty;
    objectClass->set_property = graphicsLayerActorSetProperty;
    objectClass->dispose = graphicsLayerActorDispose;

    actorClass->allocate = graphicsLayerActorAllocate;
    actorClass->apply_transform = graphicsLayerActorApplyTransform;
    actorClass->paint = graphicsLayerActorPaint;

    g_type_class_add_private(klass, sizeof(GraphicsLayerActorPrivate));

    GParamSpec* pspec = g_param_spec_float("translate-x", "Translate X", "Translation value for the X axis", -G_MAXFLOAT, G_MAXFLOAT, 0.0, static_cast<GParamFlags>(G_PARAM_READWRITE));
    g_object_class_install_property(objectClass, PropertyTranslateX, pspec);

    pspec = g_param_spec_float("translate-y", "Translate Y", "Translation value for the Y ayis", -G_MAXFLOAT, G_MAXFLOAT, 0.0, static_cast<GParamFlags>(G_PARAM_READWRITE));
    g_object_class_install_property(objectClass, PropertyTranslateY, pspec);
}

static void graphics_layer_actor_init(GraphicsLayerActor* self)
{
    self->priv = GRAPHICS_LAYER_ACTOR_GET_PRIVATE(self);

    clutter_actor_set_reactive(CLUTTER_ACTOR(self), FALSE);

    self->priv->flatten = true;

    // Default used by GraphicsLayer.
    graphicsLayerActorSetAnchorPoint(self, 0.5, 0.5, 0.0);
}

static void graphicsLayerActorSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* pspec)
{
    GraphicsLayerActor* layer = GRAPHICS_LAYER_ACTOR(object);

    switch (propID) {
    case PropertyTranslateX:
        graphicsLayerActorSetTranslateX(layer, g_value_get_float(value));
        break;
    case PropertyTranslateY:
        graphicsLayerActorSetTranslateY(layer, g_value_get_float(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
    }
}

static void graphicsLayerActorGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* pspec)
{
    GraphicsLayerActor* layer = GRAPHICS_LAYER_ACTOR(object);

    switch (propID) {
    case PropertyTranslateX:
        g_value_set_float(value, graphicsLayerActorGetTranslateX(layer));
        break;
    case PropertyTranslateY:
        g_value_set_float(value, graphicsLayerActorGetTranslateY(layer));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, pspec);
    }
}


static void graphicsLayerActorDispose(GObject* object)
{
    GraphicsLayerActor* layer = GRAPHICS_LAYER_ACTOR(object);
    GraphicsLayerActorPrivate* priv = layer->priv;

    priv->surface.clear();

    G_OBJECT_CLASS(graphics_layer_actor_parent_class)->dispose(object);
}

// Copied from cairo.
#define MAX_IMAGE_SIZE 32767

static void graphicsLayerActorAllocate(ClutterActor* self, const ClutterActorBox* box, ClutterAllocationFlags flags)
{
    CLUTTER_ACTOR_CLASS(graphics_layer_actor_parent_class)->allocate(self, box, flags);

    ClutterContent* canvas = clutter_actor_get_content(self);
    if (canvas)
        clutter_canvas_set_size(CLUTTER_CANVAS(canvas), clutter_actor_get_width(self), clutter_actor_get_height(self));

    // FIXME: maybe we can cache children allocation and not call
    // allocate on them this often?
    GOwnPtr<GList> children(clutter_actor_get_children(self));
    for (GList* child = children.get(); child; child = child->next) {
        ClutterActor* childActor = CLUTTER_ACTOR(child->data);

        float childWidth = clutter_actor_get_width(childActor);
        float childHeight = clutter_actor_get_height(childActor);

        ClutterActorBox childBox;
        childBox.x1 = clutter_actor_get_x(childActor);
        childBox.y1 = clutter_actor_get_y(childActor);
        childBox.x2 = childBox.x1 + childWidth;
        childBox.y2 = childBox.y1 + childHeight;

        clutter_actor_allocate(childActor, &childBox, flags);
    }
}

static void graphicsLayerActorApplyTransform(ClutterActor* actor, CoglMatrix* matrix)
{
    GraphicsLayerActorPrivate* priv = GRAPHICS_LAYER_ACTOR(actor)->priv;

    // Apply translation and scrolling as a single translation. These
    // need to come before anything else, otherwise they'll be
    // affected by other operations such as scaling, which is not what
    // we want.
    float translateX = priv->scrollX + priv->translateX;
    float translateY = priv->scrollY + priv->translateY;

    if (translateX || translateY)
        cogl_matrix_translate(matrix, translateX, translateY, 0);

    CoglMatrix modelViewTransform = TransformationMatrix();
    CLUTTER_ACTOR_CLASS(graphics_layer_actor_parent_class)->apply_transform(actor, &modelViewTransform);

    if (priv->flatten)
        modelViewTransform = TransformationMatrix(&modelViewTransform).to2dTransform();
    cogl_matrix_multiply(matrix, matrix, &modelViewTransform);
}

static void graphicsLayerActorPaint(ClutterActor* actor)
{
    GOwnPtr<GList> children(clutter_actor_get_children(actor));
    for (GList* child = children.get(); child; child = child->next)
        clutter_actor_paint(CLUTTER_ACTOR(child->data));
}

static gboolean graphicsLayerActorDraw(ClutterCanvas* texture, cairo_t* cr, gint width, gint height, GraphicsLayerActor* layer)
{
    if (!width || !height)
        return FALSE;

    GraphicsContext context(cr);
    context.clearRect(FloatRect(0, 0, width, height));

    GraphicsLayerActorPrivate* priv = layer->priv;
    if (priv->surface) {
        gint surfaceWidth = cairo_image_surface_get_width(priv->surface.get());
        gint surfaceHeight = cairo_image_surface_get_height(priv->surface.get());

        FloatRect srcRect(0.0, 0.0, static_cast<float>(surfaceWidth), static_cast<float>(surfaceHeight));
        FloatRect destRect(0.0, 0.0, width, height);
        context.platformContext()->drawSurfaceToContext(priv->surface.get(), destRect, srcRect, &context);
    }

    if (priv->layerType == GraphicsLayerClutter::LayerTypeWebLayer)
        drawLayerContents(CLUTTER_ACTOR(layer), context);

    return TRUE;
}

static void graphicsLayerActorUpdateTexture(GraphicsLayerActor* layer)
{
    GraphicsLayerActorPrivate* priv = layer->priv;
    ASSERT(priv->layerType != GraphicsLayerClutter::LayerTypeVideoLayer);

    ClutterActor* actor = CLUTTER_ACTOR(layer);
    GRefPtr<ClutterContent> canvas = clutter_actor_get_content(actor);
    if (canvas) {
        // Nothing needs a texture, remove the one we have, if any.
        if (!priv->drawsContent && !priv->surface) {
            g_signal_handlers_disconnect_by_func(canvas.get(), reinterpret_cast<void*>(graphicsLayerActorDraw), layer);
            clutter_actor_set_content(actor, 0);
        }
        return;
    }

    // We should have a texture, so create one.
    canvas = adoptGRef(clutter_canvas_new());
    clutter_actor_set_content(actor, canvas.get());

    int width = std::max(static_cast<int>(ceilf(clutter_actor_get_width(actor))), 1);
    int height = std::max(static_cast<int>(ceilf(clutter_actor_get_height(actor))), 1);
    clutter_canvas_set_size(CLUTTER_CANVAS(canvas.get()), width, height);

    g_signal_connect(canvas.get(), "draw", G_CALLBACK(graphicsLayerActorDraw), layer);
}

// Draw content into the layer.
static void drawLayerContents(ClutterActor* actor, GraphicsContext& context)
{
    GraphicsLayerActorPrivate* priv = GRAPHICS_LAYER_ACTOR(actor)->priv;

    if (!priv->drawsContent || !priv->layerClient)
        return;

    int width = static_cast<int>(clutter_actor_get_width(actor));
    int height = static_cast<int>(clutter_actor_get_height(actor));
    IntRect clip(0, 0, width, height);

    // Apply the painted content to the layer.
    priv->layerClient->platformClutterLayerPaintContents(context, clip);
}

static GraphicsLayerActor* graphicsLayerActorNew(GraphicsLayerClutter::LayerType type)
{
    GraphicsLayerActor* layer = GRAPHICS_LAYER_ACTOR(g_object_new(GRAPHICS_LAYER_TYPE_ACTOR, 0));
    GraphicsLayerActorPrivate* priv = layer->priv;

    priv->layerType = type;
    if (priv->layerType == GraphicsLayerClutter::LayerTypeTransformLayer)
        priv->flatten = false;

    return layer;
}

GraphicsLayerActor* graphicsLayerActorNewWithClient(GraphicsLayerClutter::LayerType type, PlatformClutterLayerClient* layerClient)
{
    GraphicsLayerActor* layer = graphicsLayerActorNew(type);
    graphicsLayerActorSetClient(layer, layerClient);

    return layer;
}

void graphicsLayerActorSetClient(GraphicsLayerActor* layer, PlatformClutterLayerClient* client)
{
    layer->priv->layerClient = client;
}

PlatformClutterLayerClient* graphicsLayerActorGetClient(GraphicsLayerActor* layer)
{
    return layer->priv->layerClient;
}

void graphicsLayerActorSetSurface(GraphicsLayerActor* layer, cairo_surface_t* surface)
{
    GraphicsLayerActorPrivate* priv = layer->priv;
    priv->surface = surface;
    graphicsLayerActorUpdateTexture(layer);
}

void graphicsLayerActorInvalidateRectangle(GraphicsLayerActor* layer, const FloatRect& dirtyRect)
{
    ClutterContent* canvas = clutter_actor_get_content(CLUTTER_ACTOR(layer));
    if (!canvas)
        return;

    // FIXME: Need to invalidate a specific area?
    clutter_content_invalidate(canvas);
}

void graphicsLayerActorSetAnchorPoint(GraphicsLayerActor* layer, float x, float y, float z)
{
    ClutterActor* actor = CLUTTER_ACTOR(layer);
    clutter_actor_set_pivot_point(actor, x, y);
    clutter_actor_set_pivot_point_z(actor, z);
}

void graphicsLayerActorSetScrollPosition(GraphicsLayerActor* layer, float x, float y)
{
    if (x > 0 || y > 0)
        return;

    GraphicsLayerActorPrivate* priv = layer->priv;
    priv->scrollX = x;
    priv->scrollY = y;

    clutter_actor_queue_redraw(CLUTTER_ACTOR(layer));
}

void graphicsLayerActorSetSublayers(GraphicsLayerActor* layer, GraphicsLayerActorList& subLayers)
{
    if (subLayers.isEmpty()) {
        clutter_actor_remove_all_children(CLUTTER_ACTOR(layer));
        return;
    }

    ClutterActor* newParentActor = CLUTTER_ACTOR(layer);
    for (size_t i = 0; i < subLayers.size(); ++i) {
        ClutterActor* childActor = CLUTTER_ACTOR(subLayers[i].get());
        ClutterActor* oldParentActor = clutter_actor_get_parent(childActor);
        if (oldParentActor) {
            if (oldParentActor == newParentActor)
                continue;
            clutter_actor_remove_child(oldParentActor, childActor);
        }
        clutter_actor_add_child(newParentActor, childActor);
    }
}

void graphicsLayerActorRemoveFromSuperLayer(GraphicsLayerActor* layer)
{
    ClutterActor* actor = CLUTTER_ACTOR(layer);
    ClutterActor* parentActor = clutter_actor_get_parent(actor);
    if (!parentActor)
        return;

    clutter_actor_remove_child(parentActor, actor);
}

GraphicsLayerClutter::LayerType graphicsLayerActorGetLayerType(GraphicsLayerActor* layer)
{
    GraphicsLayerActorPrivate* priv = layer->priv;
    return priv->layerType;
}

void graphicsLayerActorSetLayerType(GraphicsLayerActor* layer, GraphicsLayerClutter::LayerType layerType)
{
    GraphicsLayerActorPrivate* priv = layer->priv;
    priv->layerType = layerType;
}

void graphicsLayerActorSetTranslateX(GraphicsLayerActor* layer, float value)
{
    GraphicsLayerActorPrivate* priv = layer->priv;
    priv->translateX = value;
    clutter_actor_queue_redraw(CLUTTER_ACTOR(layer));
}

float graphicsLayerActorGetTranslateX(GraphicsLayerActor* layer)
{
    GraphicsLayerActorPrivate* priv = layer->priv;
    return priv->translateX;
}

void graphicsLayerActorSetTranslateY(GraphicsLayerActor* layer, float value)
{
    GraphicsLayerActorPrivate* priv = layer->priv;
    priv->translateY = value;
    clutter_actor_queue_redraw(CLUTTER_ACTOR(layer));
}

float graphicsLayerActorGetTranslateY(GraphicsLayerActor* layer)
{
    GraphicsLayerActorPrivate* priv = layer->priv;
    return priv->translateY;
}

void graphicsLayerActorSetDrawsContent(GraphicsLayerActor* layer, bool drawsContent)
{
    GraphicsLayerActorPrivate* priv = layer->priv;

    if (drawsContent == priv->drawsContent)
        return;

    priv->drawsContent = drawsContent;

    graphicsLayerActorUpdateTexture(layer);
}

void graphicsLayerActorSetFlatten(GraphicsLayerActor* layer, bool flatten)
{
    GraphicsLayerActorPrivate* priv = layer->priv;
    if (flatten == priv->flatten)
        return;

    priv->flatten = flatten;
}

void graphicsLayerActorSetMasksToBounds(GraphicsLayerActor* layer, bool masksToBounds)
{
    ClutterActor* actor = CLUTTER_ACTOR(layer);
    if (masksToBounds)
        clutter_actor_set_clip(actor, 0, 0, clutter_actor_get_width(actor), clutter_actor_get_height(actor));
    else
        clutter_actor_remove_clip(actor);
}

WebCore::PlatformClutterAnimation* graphicsLayerActorGetAnimationForKey(GraphicsLayerActor* layer, const String key)
{
    return static_cast<WebCore::PlatformClutterAnimation*>(g_object_get_data(G_OBJECT(layer), key.utf8().data()));
}

#endif // USE(ACCELERATED_COMPOSITING)
