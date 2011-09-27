/*
 * Copyright (C) 2011 Igalia S.L.
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
#include "WebKitWebContext.h"

#include <WebKit2/WKContext.h>
#include <WebKit2/WKType.h>

struct _WebKitWebContextPrivate {
    WKContextRef context;
};

G_DEFINE_TYPE(WebKitWebContext, webkit_web_context, G_TYPE_OBJECT)

static void webkitWebContextFinalize(GObject* object)
{
    WebKitWebContext* context = WEBKIT_WEB_CONTEXT(object);

    WKRelease(context->priv->context);
    context->priv->context = 0;
}

static void webkit_web_context_init(WebKitWebContext* webContext)
{
    WebKitWebContextPrivate* priv = G_TYPE_INSTANCE_GET_PRIVATE(webContext, WEBKIT_TYPE_WEB_CONTEXT, WebKitWebContextPrivate);
    webContext->priv = priv;
    new (priv) WebKitWebContextPrivate();
}

static void webkit_web_context_class_init(WebKitWebContextClass* webContextClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(webContextClass);
    gObjectClass->finalize = webkitWebContextFinalize;

    g_type_class_add_private(webContextClass, sizeof(WebKitWebContextPrivate));
}


static gpointer createDefaultWebContext(gpointer)
{
    WebKitWebContext* webContext = WEBKIT_WEB_CONTEXT(g_object_new(WEBKIT_TYPE_WEB_CONTEXT, NULL));
    webContext->priv->context = WKContextGetSharedProcessContext();
    WKContextSetCacheModel(webContext->priv->context, kWKCacheModelPrimaryWebBrowser);
    return webContext;
}

/**
 * webkit_web_context_get_default:
 *
 * Gets the default web context
 *
 * Returns: (transfer none) a #WebKitWebContext
 */
WebKitWebContext* webkit_web_context_get_default(void)
{
    static GOnce onceInit = G_ONCE_INIT;
    return WEBKIT_WEB_CONTEXT(g_once(&onceInit, createDefaultWebContext, 0));
}


