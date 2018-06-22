/*
 * Copyright (C) 2017 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#pragma once

#include <glib.h>
#include <wtf/Compiler.h>

#define WEBKIT_PARAM_READABLE (static_cast<GParamFlags>(G_PARAM_READABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB))
#define WEBKIT_PARAM_WRITABLE (static_cast<GParamFlags>(G_PARAM_WRITABLE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB))
#define WEBKIT_PARAM_READWRITE (static_cast<GParamFlags>(G_PARAM_READWRITE | G_PARAM_STATIC_NAME | G_PARAM_STATIC_NICK | G_PARAM_STATIC_BLURB))

#define WEBKIT_DEFINE_ASYNC_DATA_STRUCT(structName) \
static structName* create##structName() \
{ \
    structName* data = static_cast<structName*>(fastZeroedMalloc(sizeof(structName))); \
    new (data) structName(); \
    return data; \
} \
static void destroy##structName(structName* data) \
{ \
    data->~structName(); \
    fastFree(data); \
}

#define WEBKIT_DEFINE_TYPE(TypeName, type_name, TYPE_PARENT) _WEBKIT_DEFINE_TYPE_EXTENDED(TypeName, type_name, TYPE_PARENT, 0, { })
#define WEBKIT_DEFINE_ABSTRACT_TYPE(TypeName, type_name, TYPE_PARENT) _WEBKIT_DEFINE_TYPE_EXTENDED(TypeName, type_name, TYPE_PARENT, G_TYPE_FLAG_ABSTRACT, { })
#define WEBKIT_DEFINE_TYPE_WITH_CODE(TypeName, type_name, TYPE_PARENT, Code) _WEBKIT_DEFINE_TYPE_EXTENDED_BEGIN(TypeName, type_name, TYPE_PARENT, 0) {Code;} _WEBKIT_DEFINE_TYPE_EXTENDED_END()

#define _WEBKIT_DEFINE_TYPE_EXTENDED(TypeName, type_name, TYPE_PARENT, flags, Code) _WEBKIT_DEFINE_TYPE_EXTENDED_BEGIN(TypeName, type_name, TYPE_PARENT, flags) {Code;} _WEBKIT_DEFINE_TYPE_EXTENDED_END()
#define _WEBKIT_DEFINE_TYPE_EXTENDED_BEGIN(TypeName, type_name, TYPE_PARENT, flags) \
\
static void type_name##_class_init(TypeName##Class* klass); \
static GType type_name##_get_type_once(void); \
static gpointer type_name##_parent_class = 0; \
static void type_name##_finalize(GObject* object) \
{ \
    TypeName* self = (TypeName*)object; \
    self->priv->~TypeName##Private(); \
    G_OBJECT_CLASS(type_name##_parent_class)->finalize(object); \
} \
\
static void type_name##_class_intern_init(gpointer klass, gpointer) \
{ \
    GObjectClass* gObjectClass = G_OBJECT_CLASS(klass); \
    g_type_class_add_private(klass, sizeof(TypeName##Private)); \
    type_name##_parent_class = g_type_class_peek_parent(klass); \
    type_name##_class_init((TypeName##Class*)klass); \
    gObjectClass->finalize = type_name##_finalize; \
} \
\
static void type_name##_init(TypeName* self, gpointer) \
{ \
    TypeName##Private* priv = G_TYPE_INSTANCE_GET_PRIVATE(self, type_name##_get_type(), TypeName##Private); \
    self->priv = priv; \
    new (priv) TypeName##Private(); \
} \
\
GType type_name##_get_type(void) \
{ \
    static volatile gsize g_define_type_id__volatile = 0; \
    if (g_once_init_enter(&g_define_type_id__volatile)) { \
        GType g_define_type_id = type_name##_get_type_once(); \
        g_once_init_leave(&g_define_type_id__volatile, g_define_type_id); \
    } \
    return g_define_type_id__volatile; \
} /* Closes type_name##_get_type(). */ \
\
NEVER_INLINE static GType type_name##_get_type_once(void) \
{ \
    GType g_define_type_id =  \
        g_type_register_static_simple( \
            TYPE_PARENT, \
            g_intern_static_string(#TypeName), \
            sizeof(TypeName##Class), \
            (GClassInitFunc)(void (*)(void))type_name##_class_intern_init, \
            sizeof(TypeName), \
            (GInstanceInitFunc)(void (*)(void))type_name##_init, \
            (GTypeFlags)flags); \
    /* Custom code follows. */
#define _WEBKIT_DEFINE_TYPE_EXTENDED_END() \
    return g_define_type_id; \
} /* Closes type_name##_get_type_once() */
