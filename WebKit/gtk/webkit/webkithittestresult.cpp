/*
 * Copyright (C) 2009 Collabora Ltd.
 * Copyright (C) 2009 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
#include "webkithittestresult.h"

#include "GOwnPtr.h"
#include "webkitenumtypes.h"
#include "webkitprivate.h"
#include <wtf/text/CString.h>

#include <glib/gi18n-lib.h>

/**
 * SECTION:webkithittestresult
 * @short_description: The target of a mouse event
 *
 * This class holds context information about the coordinates
 * specified by a GDK event.
 */

G_DEFINE_TYPE(WebKitHitTestResult, webkit_hit_test_result, G_TYPE_OBJECT)

struct _WebKitHitTestResultPrivate {
    guint context;
    char* linkURI;
    char* imageURI;
    char* mediaURI;
};

#define WEBKIT_HIT_TEST_RESULT_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), WEBKIT_TYPE_HIT_TEST_RESULT, WebKitHitTestResultPrivate))

enum {
    PROP_0,

    PROP_CONTEXT,
    PROP_LINK_URI,
    PROP_IMAGE_URI,
    PROP_MEDIA_URI
};

static void webkit_hit_test_result_finalize(GObject* object)
{
    WebKitHitTestResult* web_hit_test_result = WEBKIT_HIT_TEST_RESULT(object);
    WebKitHitTestResultPrivate* priv = web_hit_test_result->priv;

    g_free(priv->linkURI);
    g_free(priv->imageURI);
    g_free(priv->mediaURI);

    G_OBJECT_CLASS(webkit_hit_test_result_parent_class)->finalize(object);
}

static void webkit_hit_test_result_get_property(GObject* object, guint propertyID, GValue* value, GParamSpec* pspec)
{
    WebKitHitTestResult* web_hit_test_result = WEBKIT_HIT_TEST_RESULT(object);
    WebKitHitTestResultPrivate* priv = web_hit_test_result->priv;

    switch(propertyID) {
    case PROP_CONTEXT:
        g_value_set_flags(value, priv->context);
        break;
    case PROP_LINK_URI:
        g_value_set_string(value, priv->linkURI);
        break;
    case PROP_IMAGE_URI:
        g_value_set_string(value, priv->imageURI);
        break;
    case PROP_MEDIA_URI:
        g_value_set_string(value, priv->mediaURI);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyID, pspec);
    }
}

static void webkit_hit_test_result_set_property(GObject* object, guint propertyID, const GValue* value, GParamSpec* pspec)
{
    WebKitHitTestResult* web_hit_test_result = WEBKIT_HIT_TEST_RESULT(object);
    WebKitHitTestResultPrivate* priv = web_hit_test_result->priv;

    switch(propertyID) {
    case PROP_CONTEXT:
        priv->context = g_value_get_flags(value);
        break;
    case PROP_LINK_URI:
        g_free (priv->linkURI);
        priv->linkURI = g_value_dup_string(value);
        break;
    case PROP_IMAGE_URI:
        g_free (priv->imageURI);
        priv->imageURI = g_value_dup_string(value);
        break;
    case PROP_MEDIA_URI:
        g_free (priv->mediaURI);
        priv->mediaURI = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyID, pspec);
    }
}

static void webkit_hit_test_result_class_init(WebKitHitTestResultClass* webHitTestResultClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(webHitTestResultClass);

    objectClass->finalize = webkit_hit_test_result_finalize;
    objectClass->get_property = webkit_hit_test_result_get_property;
    objectClass->set_property = webkit_hit_test_result_set_property;

    webkit_init();

    /**
     * WebKitHitTestResult:context:
     *
     * Flags indicating the kind of target that received the event.
     *
     * Since: 1.1.15
     */
    g_object_class_install_property(objectClass, PROP_CONTEXT,
                                    g_param_spec_flags("context",
                                                       _("Context"),
                                                       _("Flags indicating the kind of target that received the event."),
                                                       WEBKIT_TYPE_HIT_TEST_RESULT_CONTEXT,
                                                       WEBKIT_HIT_TEST_RESULT_CONTEXT_DOCUMENT,
                                                       static_cast<GParamFlags>((WEBKIT_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY))));

    /**
     * WebKitHitTestResult:link-uri:
     *
     * The URI to which the target that received the event points, if any.
     *
     * Since: 1.1.15
     */
    g_object_class_install_property(objectClass, PROP_LINK_URI,
                                    g_param_spec_string("link-uri",
                                                        _("Link URI"),
                                                        _("The URI to which the target that received the event points, if any."),
                                                        NULL,
                                                        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitHitTestResult:image-uri:
     *
     * The URI of the image that is part of the target that received the event, if any.
     *
     * Since: 1.1.15
     */
    g_object_class_install_property(objectClass, PROP_IMAGE_URI,
                                    g_param_spec_string("image-uri",
                                                        _("Image URI"),
                                                        _("The URI of the image that is part of the target that received the event, if any."),
                                                        NULL,
                                                        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitHitTestResult:media-uri:
     *
     * The URI of the media that is part of the target that received the event, if any.
     *
     * Since: 1.1.15
     */
    g_object_class_install_property(objectClass, PROP_MEDIA_URI,
                                    g_param_spec_string("media-uri",
                                                        _("Media URI"),
                                                        _("The URI of the media that is part of the target that received the event, if any."),
                                                        NULL,
                                                        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE|G_PARAM_CONSTRUCT_ONLY)));

    g_type_class_add_private(webHitTestResultClass, sizeof(WebKitHitTestResultPrivate));
}

static void webkit_hit_test_result_init(WebKitHitTestResult* web_hit_test_result)
{
    web_hit_test_result->priv = WEBKIT_HIT_TEST_RESULT_GET_PRIVATE(web_hit_test_result);
}
