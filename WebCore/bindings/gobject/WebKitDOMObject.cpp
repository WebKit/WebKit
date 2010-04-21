/*
 * Copyright (C) 2008 Luke Kenneth Casson Leighton <lkcl@lkcl.net>
 * Copyright (C) 2008 Martin Soto <soto@freedesktop.org>
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Apple Inc.
 * Copyright (C) 2009 Igalia S.L.
 */
#include "config.h"
#include "WebKitDOMObject.h"

#include "glib-object.h"
#include "WebKitDOMBinding.h"

G_DEFINE_TYPE(WebKitDOMObject, webkit_dom_object, G_TYPE_OBJECT);

static void webkit_dom_object_init(WebKitDOMObject* object)
{
}

static void webkit_dom_object_class_init(WebKitDOMObjectClass* klass)
{
}

