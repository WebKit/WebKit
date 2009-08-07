/*
 * Copyright (C) 2008, 2009 Luke Kenneth Casson Leighton <lkcl@lkcl.net>
 * Copyright (C) 2008 Martin Soto <soto@freedesktop.org>
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Apple Inc.
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

#include "GDOMBinding.h"
#include "GdomDOMObject.h"
#include "GdomDOMObjectPrivate.h"

extern "C" {

G_DEFINE_TYPE(GdomDOMObject, gdom_dom_object, G_TYPE_OBJECT);

static void gdom_dom_object_init(GdomDOMObject* request)
{
    GdomDOMObjectPrivate* priv = GDOM_DOM_OBJECT_GET_PRIVATE(request);
    request->priv = priv;
}

static void gdom_dom_object_finalize(GObject* object)
{
    G_OBJECT_CLASS(gdom_dom_object_parent_class)->finalize(object);
}

static void gdom_dom_object_class_init(GdomDOMObjectClass* requestClass)
{
    G_OBJECT_CLASS(requestClass)->finalize = gdom_dom_object_finalize;

    g_type_class_add_private(requestClass, sizeof(GdomDOMObjectPrivate));
}

} /* extern "C" */

