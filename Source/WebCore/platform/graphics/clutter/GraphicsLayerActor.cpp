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

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsLayerActor.h"

#include "GraphicsContext.h"
#include "GraphicsLayerClutter.h"
#include "PlatformClutterLayerClient.h"
#include "PlatformContextCairo.h"
#include "RefPtrCairo.h"

using namespace WebCore;

G_DEFINE_TYPE(GraphicsLayerActor, graphics_layer_actor, CLUTTER_TYPE_RECTANGLE)

#define GRAPHICS_LAYER_ACTOR_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), GRAPHICS_LAYER_TYPE_ACTOR, GraphicsLayerActorPrivate))

struct _GraphicsLayerActorPrivate {
    GraphicsLayerClutter::LayerType layerType;
    gboolean allocating;

    ClutterActor* texture;
    RefPtr<cairo_surface_t> surface;
    CoglMatrix* matrix;

    PlatformClutterLayerClient* layerClient;

    gboolean drawsContent;

    float anchorX;
    float anchorY;
    float anchorZ;

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
static void graphicsLayerActorPick(ClutterActor*, const ClutterColor*);

static void graphicsLayerActorAdded(ClutterContainer*, ClutterActor*, gpointer data);
static void graphicsLayerActorRemoved(ClutterContainer*, ClutterActor*, gpointer data);
static gboolean graphicsLayerActorDraw(ClutterCairoTexture*, cairo_t*, GraphicsLayerActor*);
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
    GraphicsLayerActorPrivate* priv = self->priv = GRAPHICS_LAYER_ACTOR_GET_PRIVATE(self);

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

    if (priv->texture) {
        clutter_actor_destroy(priv->texture);
        priv->texture = 0;
    }

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

    if (priv->texture) {
        // The texture occupies the whole area, but is positioned at 0,0
        // relative to the layer actor.
        ClutterActorBox textureBox = { 0.0, 0.0, box->x2 - box->x1, box->y2 - box->y1 };

        // protect against 0x0
        if (!textureBox.x2)
            textureBox.x2 = 1;

        if (!textureBox.y2)
            textureBox.y2 = 1;

        clutter_actor_allocate(priv->texture, &textureBox, flags);
    }

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

    float width = clutter_actor_get_width(actor);
    float height = clutter_actor_get_height(actor);
    if (width <= 1.0 || height <= 1.0)
        return;

    float pivotX, pivotY;
    pivotX = width * priv->anchorX;
    pivotY = height * priv->anchorY;

    if (priv->matrix) {
        CoglMatrix* localMatrix;
        // CSS3 tranform-style can be changed on the fly, 
        // so we have to copy priv->matrix in order to recover z-axis. 
        localMatrix = cogl_matrix_copy(priv->matrix);

        cogl_matrix_translate(matrix, pivotX, pivotY, priv->anchorZ);
        cogl_matrix_multiply(matrix, matrix, localMatrix);
        cogl_matrix_translate(matrix, -pivotX, -pivotY, -priv->anchorZ);
        cogl_matrix_free(localMatrix);
    }
}

static void graphicsLayerActorPaint(ClutterActor* actor)
{
    GraphicsLayerActor* graphicsLayer = GRAPHICS_LAYER_ACTOR(actor);
    GraphicsLayerActorPrivate* priv = GRAPHICS_LAYER_ACTOR(actor)->priv;

    // Paint the texture in case we have a border. This will be the case when debugging borders
    // are turned on that we want to see.
    if (priv->texture)
        clutter_actor_paint(priv->texture);

    GList* list;
    for (list = graphicsLayer->children; list; list = list->next) {
        ClutterActor* child = CLUTTER_ACTOR(list->data);
        clutter_actor_paint(child);
    }
}

static gboolean graphicsLayerActorDraw(ClutterCairoTexture* texture, cairo_t* cr, GraphicsLayerActor* layer)
{
    ClutterActor* actor = CLUTTER_ACTOR(layer);
    float width = clutter_actor_get_width(actor);
    float height = clutter_actor_get_height(actor);

    if (!width || !height)
        return FALSE;

    GraphicsLayerActorPrivate* priv = layer->priv;
    GraphicsContext context(cr);

    clutter_cairo_texture_clear(texture);

    if (priv->surface) {
        gint surfaceWidth = cairo_image_surface_get_width(priv->surface.get());
        gint surfaceHeight = cairo_image_surface_get_height(priv->surface.get());

        FloatRect srcRect(0.0, 0.0, static_cast<float>(surfaceWidth), static_cast<float>(surfaceHeight));
        FloatRect destRect(0.0, 0.0, width, height);
        context.platformContext()->drawSurfaceToContext(priv->surface.get(), destRect, srcRect, &context);
    }

    if (priv->layerType == GraphicsLayerClutter::LayerTypeWebLayer)
        drawLayerContents(actor, context);

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

    // Nothing needs a texture, remove the one we have, if any.
    if (!priv->drawsContent && !priv->surface) {
        if (!priv->texture)
            return;

        g_signal_handlers_disconnect_by_func(priv->texture, reinterpret_cast<void*>(graphicsLayerActorDraw), layer);
        clutter_actor_unparent(priv->texture);
        priv->texture = 0;
        return;
    }

    // We need a texture, but already have one!
    if (priv->texture)
        return;

    // We need a texture, so create it.
    ClutterActor* actor = CLUTTER_ACTOR(layer);
    int width = ceilf(clutter_actor_get_width(actor));
    int height = ceilf(clutter_actor_get_height(actor));

    priv->texture = clutter_cairo_texture_new(width > 0 ? width : 1, height > 0 ? height : 1);
    clutter_cairo_texture_set_auto_resize(CLUTTER_CAIRO_TEXTURE(priv->texture), TRUE);
    clutter_actor_set_parent(priv->texture, actor);

    g_signal_connect(priv->texture, "draw", G_CALLBACK(graphicsLayerActorDraw), layer);
}

// Draw content into the layer.
static void drawLayerContents(ClutterActor* actor, GraphicsContext& context)
{
    GraphicsLayerActorPrivate* priv = GRAPHICS_LAYER_ACTOR(actor)->priv;

    if (!priv->drawsContent || !priv->layerClient)
        return;

    float width = clutter_actor_get_width(actor);
    float height = clutter_actor_get_height(actor);
    IntRect clip(0, 0, width, height);

    // Apply the painted content to the layer.
    priv->layerClient->platformClutterLayerPaintContents(context, clip);
}


GraphicsLayerActor* graphicsLayerActorNew(GraphicsLayerClutter::LayerType type)
{
    GraphicsLayerActor* layer = GRAPHICS_LAYER_ACTOR(g_object_new(GRAPHICS_LAYER_TYPE_ACTOR, 0));
    GraphicsLayerActorPrivate* priv = layer->priv;

    priv->layerType = type;

    // For video layers we don't want the cairo texture, but a regular one.
    if (priv->layerType == GraphicsLayerClutter::LayerTypeVideoLayer) {
        priv->texture = clutter_texture_new();
        clutter_actor_set_parent(priv->texture, CLUTTER_ACTOR(layer));
    }

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
    for (; children; children = children->next) {
        // We only want to remove sublayers, not our own composite actors.
        if (children->data == layer->priv->texture)
            continue;

        clutter_actor_remove_child(CLUTTER_ACTOR(layer), CLUTTER_ACTOR(children->data));
    }
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
    GraphicsLayerActorPrivate* priv = layer->priv;

    if (!priv->texture)
        return;

    cairo_rectangle_int_t rect(enclosingIntRect(dirtyRect));
    clutter_cairo_texture_invalidate_rectangle(CLUTTER_CAIRO_TEXTURE(priv->texture), &rect);
}

void graphicsLayerActorSetTransform(GraphicsLayerActor* layer, const CoglMatrix* matrix) 
{
    GraphicsLayerActorPrivate* priv = layer->priv;

    if (priv->matrix) {
        cogl_matrix_free(priv->matrix);
        priv->matrix = 0;
        clutter_actor_queue_redraw(CLUTTER_ACTOR(layer));
    }

    CoglMatrix identity;
    cogl_matrix_init_identity(&identity);
    if (cogl_matrix_equal((CoglMatrix*)&identity, (CoglMatrix*)matrix))
        return;

    if (priv->matrix)
        cogl_matrix_free(priv->matrix);

    priv->matrix = cogl_matrix_copy(matrix);
    clutter_actor_queue_redraw(CLUTTER_ACTOR(layer));
}

void graphicsLayerActorSetAnchorPoint(GraphicsLayerActor* layer, float x, float y, float z)
{
    GraphicsLayerActorPrivate* priv = layer->priv;

    priv->anchorX = x;
    priv->anchorY = y;
    priv->anchorZ = z;

    ClutterActor* actor = CLUTTER_ACTOR(layer);

    float width, height;
    clutter_actor_get_size(actor, &width, &height);
    clutter_actor_set_anchor_point(actor, width * priv->anchorX, height * priv->anchorY);
}

void graphicsLayerActorGetAnchorPoint(GraphicsLayerActor* layer, float* x, float* y, float* z)
{
    GraphicsLayerActorPrivate* priv = layer->priv;
    if (x)
        *x = priv->anchorX;

    if (y)
        *y = priv->anchorY;

    if (z)
        *z = priv->anchorZ;
}

void graphicsLayerActorSetScrollPosition(GraphicsLayerActor* layer, float x, float y)
{
    GraphicsLayerActorPrivate* priv = layer->priv;

    if (x > 0 || y > 0)
        return;

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
    clutter_actor_set_parent(childLayer, CLUTTER_ACTOR(layer));
    clutter_actor_queue_relayout(CLUTTER_ACTOR(layer));

    g_object_unref(childLayer);
}

void graphicsLayerActorSetSublayers(GraphicsLayerActor* layer, GraphicsLayerActorList& subLayers)
{
    if (!subLayers.size()) {
        graphicsLayerActorRemoveAll(layer);
        return;
    }

    for (size_t i = 0; i < subLayers.size(); ++i) {
        ClutterActor* layerActor = CLUTTER_ACTOR(subLayers[i].get());
        clutter_container_add_actor(CLUTTER_CONTAINER(layer), layerActor);
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

#endif // USE(ACCELERATED_COMPOSITING)
