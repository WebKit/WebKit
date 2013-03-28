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
#include <wtf/text/CString.h>

using namespace WebCore;

G_DEFINE_TYPE(GraphicsLayerActor, graphics_layer_actor, CLUTTER_TYPE_ACTOR)

#define GRAPHICS_LAYER_ACTOR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GRAPHICS_LAYER_TYPE_ACTOR, GraphicsLayerActorPrivate))

struct _GraphicsLayerActorPrivate {
    GraphicsLayerClutter::LayerType layerType;
    gboolean allocating;

    RefPtr<cairo_surface_t> surface;
    CoglMatrix* matrix;

    PlatformClutterLayerClient* layerClient;

    gboolean drawsContent;

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

static void graphicsLayerActorAdded(ClutterContainer*, ClutterActor*, gpointer data);
static void graphicsLayerActorRemoved(ClutterContainer*, ClutterActor*, gpointer data);
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
    actorClass->apply_transform = graphicsLayerActorApplyTransform;
    actorClass->allocate = graphicsLayerActorAllocate;
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

    // Default used by GraphicsLayer.
    graphicsLayerActorSetAnchorPoint(self, 0.5, 0.5, 0.0);

    g_signal_connect(self, "actor-added", G_CALLBACK(graphicsLayerActorAdded), 0);
    g_signal_connect(self, "actor-removed", G_CALLBACK(graphicsLayerActorRemoved), 0);
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

    if (priv->matrix)
        cogl_matrix_free(priv->matrix);

    G_OBJECT_CLASS(graphics_layer_actor_parent_class)->dispose(object);
}

// Copied from cairo.
#define MAX_IMAGE_SIZE 32767

static void graphicsLayerActorAllocate(ClutterActor* self, const ClutterActorBox* box, ClutterAllocationFlags flags)
{
    GraphicsLayerActor* layer = GRAPHICS_LAYER_ACTOR(self);
    GraphicsLayerActorPrivate* priv = layer->priv;

    priv->allocating = TRUE;

    CLUTTER_ACTOR_CLASS(graphics_layer_actor_parent_class)->allocate(self, box, flags);

    ClutterContent* canvas = clutter_actor_get_content(self);
    if (canvas)
        clutter_canvas_set_size(CLUTTER_CANVAS(canvas), clutter_actor_get_width(self), clutter_actor_get_height(self));

    // FIXME: maybe we can cache children allocation and not call
    // allocate on them this often?
    for (GList* list = layer->children; list; list = list->next) {
        ClutterActor* child = CLUTTER_ACTOR(list->data);

        float childWidth = clutter_actor_get_width(child);
        float childHeight = clutter_actor_get_height(child);

        ClutterActorBox childBox;
        childBox.x1 = clutter_actor_get_x(child);
        childBox.y1 = clutter_actor_get_y(child);
        childBox.x2 = childBox.x1 + childWidth;
        childBox.y2 = childBox.y1 + childHeight;

        clutter_actor_allocate(child, &childBox, flags);
    }

    priv->allocating = FALSE;
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

    CLUTTER_ACTOR_CLASS(graphics_layer_actor_parent_class)->apply_transform(actor, matrix);

    if (priv->matrix) {
        float width = 0, height = 0;
        clutter_actor_get_size(actor, &width, &height);
        if (width <= 1.0 || height <= 1.0)
            return;

        // The pivot of actor is a normalized value, so we need an actual anchor position
        // in actor's local coordinate system for translating.
        float anchorX = 0, anchorY = 0, anchorZ = 0;
        graphicsLayerActorGetAnchorPoint(GRAPHICS_LAYER_ACTOR(actor), &anchorX, &anchorY, &anchorZ);
        anchorX *= width;
        anchorY *= height;

        // CSS3 tranform-style can be changed on the fly, 
        // so we have to copy priv->matrix in order to recover z-axis. 
        CoglMatrix* localMatrix = cogl_matrix_copy(priv->matrix);

        cogl_matrix_translate(matrix, anchorX, anchorY, anchorZ);
        cogl_matrix_multiply(matrix, matrix, localMatrix);
        cogl_matrix_translate(matrix, -anchorX, -anchorY, -anchorZ);
        cogl_matrix_free(localMatrix);
    }
}

static void graphicsLayerActorPaint(ClutterActor* actor)
{
    GraphicsLayerActor* graphicsLayer = GRAPHICS_LAYER_ACTOR(actor);

    for (GList* list = graphicsLayer->children; list; list = list->next) {
        ClutterActor* child = CLUTTER_ACTOR(list->data);
        clutter_actor_paint(child);
    }
}

static gboolean graphicsLayerActorDraw(ClutterCanvas* texture, cairo_t* cr, gint width, gint height, GraphicsLayerActor* layer)
{
    if (!width || !height)
        return FALSE;

    GraphicsLayerActorPrivate* priv = layer->priv;
    GraphicsContext context(cr);
    context.clearRect(FloatRect(0, 0, width, height));

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

static void graphicsLayerActorAdded(ClutterContainer* container, ClutterActor* actor, gpointer data)
{
    GraphicsLayerActor* graphicsLayer = GRAPHICS_LAYER_ACTOR(container);
    graphicsLayer->children = g_list_append(graphicsLayer->children, actor);
}

static void graphicsLayerActorRemoved(ClutterContainer* container, ClutterActor* actor, gpointer data)
{
    GraphicsLayerActor* graphicsLayer = GRAPHICS_LAYER_ACTOR(container);
    graphicsLayer->children = g_list_remove(graphicsLayer->children, actor);
}

static void graphicsLayerActorUpdateTexture(GraphicsLayerActor* layer)
{
    GraphicsLayerActorPrivate* priv = layer->priv;
    ASSERT(priv->layerType != GraphicsLayerClutter::LayerTypeVideoLayer);

    ClutterActor* actor = CLUTTER_ACTOR(layer);
    ClutterContent* canvas = clutter_actor_get_content(actor);
    if (canvas) {
        // Nothing needs a texture, remove the one we have, if any.
        if (!priv->drawsContent && !priv->surface) {
            g_signal_handlers_disconnect_by_func(canvas, reinterpret_cast<void*>(graphicsLayerActorDraw), layer);
            g_object_unref(canvas);
        }
        return;
    }

    // We should have a texture, so create one.
    int width = ceilf(clutter_actor_get_width(actor));
    int height = ceilf(clutter_actor_get_height(actor));

    canvas = clutter_canvas_new();
    clutter_actor_set_content(actor, canvas);
    clutter_canvas_set_size(CLUTTER_CANVAS(canvas), width > 0 ? width : 1, height > 0 ? height : 1);
    g_object_unref(canvas);
    
    g_signal_connect(canvas, "draw", G_CALLBACK(graphicsLayerActorDraw), layer);
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


GraphicsLayerActor* graphicsLayerActorNew(GraphicsLayerClutter::LayerType type)
{
    GraphicsLayerActor* layer = GRAPHICS_LAYER_ACTOR(g_object_new(GRAPHICS_LAYER_TYPE_ACTOR, 0));
    layer->priv->layerType = type;

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

void graphicsLayerActorRemoveAll(GraphicsLayerActor* layer)
{
    g_return_if_fail(GRAPHICS_LAYER_IS_ACTOR(layer));

    GList* children = clutter_actor_get_children(CLUTTER_ACTOR(layer));
    for (; children; children = children->next)
        clutter_actor_remove_child(CLUTTER_ACTOR(layer), CLUTTER_ACTOR(children->data));
}

cairo_surface_t* graphicsLayerActorGetSurface(GraphicsLayerActor* layer)
{
    GraphicsLayerActorPrivate* priv = layer->priv;
    ASSERT(priv->surface);
    return priv->surface.get();
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

void graphicsLayerActorSetTransform(GraphicsLayerActor* layer, const CoglMatrix* matrix) 
{
    bool needToRedraw = false;

    GraphicsLayerActorPrivate* priv = layer->priv;
    if (priv->matrix) {
        cogl_matrix_free(priv->matrix);
        needToRedraw = true;
    }

    if (matrix && !cogl_matrix_is_identity(matrix)) {
        priv->matrix = cogl_matrix_copy(matrix);
        needToRedraw = true;
    } else
        priv->matrix = 0;

    if (needToRedraw)
        clutter_actor_queue_redraw(CLUTTER_ACTOR(layer));
}

void graphicsLayerActorSetAnchorPoint(GraphicsLayerActor* layer, float x, float y, float z)
{
    ClutterActor* actor = CLUTTER_ACTOR(layer);
    clutter_actor_set_pivot_point(actor, x, y);
    clutter_actor_set_pivot_point_z(actor, z);
}

void graphicsLayerActorGetAnchorPoint(GraphicsLayerActor* layer, float* x, float* y, float* z)
{
    ASSERT(x && y);

    ClutterActor* actor = CLUTTER_ACTOR(layer);
    clutter_actor_get_pivot_point(actor, x, y);
    if (z)
        *z = clutter_actor_get_pivot_point_z(actor);
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

gint graphicsLayerActorGetnChildren(GraphicsLayerActor* layer)
{
    ASSERT(GRAPHICS_LAYER_IS_ACTOR(layer));

    return g_list_length(layer->children);
}

void graphicsLayerActorReplaceSublayer(GraphicsLayerActor* layer, ClutterActor* oldChildLayer, ClutterActor* newChildLayer)
{
    ASSERT(GRAPHICS_LAYER_IS_ACTOR(layer));
    ASSERT(CLUTTER_IS_ACTOR(oldChildLayer));
    ASSERT(CLUTTER_IS_ACTOR(newChildLayer));

    clutter_actor_remove_child(CLUTTER_ACTOR(layer), oldChildLayer);
    clutter_actor_add_child(CLUTTER_ACTOR(layer), newChildLayer);
}

void graphicsLayerActorInsertSublayer(GraphicsLayerActor* layer, ClutterActor* childLayer, gint index)
{
    ASSERT(GRAPHICS_LAYER_IS_ACTOR(layer));
    ASSERT(CLUTTER_IS_ACTOR(childLayer));

    g_object_ref(childLayer);

    layer->children = g_list_insert(layer->children, childLayer, index);
    ASSERT(!clutter_actor_get_parent(childLayer));
    clutter_actor_add_child(CLUTTER_ACTOR(layer), childLayer);
    clutter_actor_queue_relayout(CLUTTER_ACTOR(layer));

    g_object_unref(childLayer);
}

void graphicsLayerActorSetSublayers(GraphicsLayerActor* layer, GraphicsLayerActorList& subLayers)
{
    if (!subLayers.size()) {
        graphicsLayerActorRemoveAll(layer);
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

void graphicsLayerActorSetDrawsContent(GraphicsLayerActor* layer, gboolean drawsContent)
{
    GraphicsLayerActorPrivate* priv = layer->priv;

    if (drawsContent == priv->drawsContent)
        return;

    priv->drawsContent = drawsContent;

    graphicsLayerActorUpdateTexture(layer);
}

gboolean graphicsLayerActorGetDrawsContent(GraphicsLayerActor* layer)
{
    return layer->priv->drawsContent;
}

WebCore::PlatformClutterAnimation* graphicsLayerActorGetAnimationForKey(GraphicsLayerActor* layer, const String key)
{
    return static_cast<WebCore::PlatformClutterAnimation*>(g_object_get_data(G_OBJECT(layer), key.utf8().data()));
}

#endif // USE(ACCELERATED_COMPOSITING)
