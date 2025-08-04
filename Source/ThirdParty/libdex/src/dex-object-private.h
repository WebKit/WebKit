/*
 * dex-object-private.h
 *
 * Copyright 2022-2023 Christian Hergert <chergert@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "dex-compat-private.h"
#include "dex-object.h"

G_BEGIN_DECLS

#define DEX_OBJECT_CLASS(klass)   G_TYPE_CHECK_CLASS_CAST(klass, DEX_TYPE_OBJECT, DexObjectClass)
#define DEX_OBJECT_GET_CLASS(obj) G_TYPE_INSTANCE_GET_CLASS(obj, DEX_TYPE_OBJECT, DexObjectClass)

#define _DEX_DEFINE_TYPE(ClassName, class_name, PARENT_TYPE, Flags)                               \
  static void G_PASTE(class_name, _class_init) (G_PASTE (ClassName, Class) *);                    \
  static void G_PASTE(class_name, _init) (ClassName *);                                           \
  static gpointer G_PASTE(class_name, _parent_class);                                             \
  static GType G_PASTE(class_name, _type);                                                        \
                                                                                                  \
  static void                                                                                     \
  G_PASTE(class_name, _class_intern_init) (gpointer klass)                                        \
  {                                                                                               \
    G_PASTE (class_name, _parent_class) = g_type_class_peek_parent (klass);                       \
    G_PASTE (class_name, _class_init) ((G_PASTE (ClassName, Class) *)klass);                      \
  }                                                                                               \
                                                                                                  \
  GType G_PASTE(class_name, _get_type) (void)                                                     \
  {                                                                                               \
    if (g_once_init_enter (&G_PASTE (class_name, _type)))                                         \
      {                                                                                           \
        GType gtype = g_type_register_static_simple (                                             \
            PARENT_TYPE,                                                                          \
            g_intern_static_string (G_STRINGIFY (ClassName)),                                     \
            sizeof (G_PASTE (ClassName, Class)),                                                  \
            (GClassInitFunc) G_PASTE(class_name, _class_intern_init),                             \
            sizeof (ClassName),                                                                   \
            (GInstanceInitFunc) G_PASTE(class_name, _init),                                       \
            Flags);                                                                               \
        g_once_init_leave (&G_PASTE (class_name, _type), gtype);                                  \
      }                                                                                           \
    return G_PASTE(class_name, _type);                                                            \
  }
#define DEX_DEFINE_ABSTRACT_TYPE(ClassName, class_name, PARENT_TYPE)                              \
  _DEX_DEFINE_TYPE(ClassName, class_name, PARENT_TYPE, G_TYPE_FLAG_ABSTRACT)
#define DEX_DEFINE_DERIVABLE_TYPE(ClassName, class_name, PARENT_TYPE)                             \
  _DEX_DEFINE_TYPE(ClassName, class_name, PARENT_TYPE, 0)
#define DEX_DEFINE_FINAL_TYPE(ClassName, class_name, PARENT_TYPE)                                 \
  _DEX_DEFINE_TYPE(ClassName, class_name, PARENT_TYPE, G_TYPE_FLAG_FINAL)

#if defined(_MSC_VER)
# define DEX_ALIGNED_BEGIN(_N) __declspec(align (_N))
# define DEX_ALIGNED_END(_N)
#else
# define DEX_ALIGNED_BEGIN(_N)
# define DEX_ALIGNED_END(_N) __attribute__ ((aligned (_N)))
#endif

typedef struct _DexWeakRef
{
  GMutex              mutex;
  struct _DexWeakRef *next;
  struct _DexWeakRef *prev;
  gpointer            mem_block;
} DexWeakRef;

void     dex_weak_ref_clear (DexWeakRef *weak_ref);
void     dex_weak_ref_init  (DexWeakRef *weak_ref,
                             gpointer    mem_block);
gpointer dex_weak_ref_get   (DexWeakRef *weak_ref);
void     dex_weak_ref_set   (DexWeakRef *weak_ref,
                             gpointer    mem_block);

DEX_ALIGNED_BEGIN (8)
typedef struct _DexObject
{
  GTypeInstance    parent_instance;
  GMutex           mutex;
  DexWeakRef      *weak_refs;
  guint            weak_refs_watermark;
  _Atomic int      ref_count;
#ifdef HAVE_SYSPROF
  gint64           ctime;
#endif
} DexObject
DEX_ALIGNED_END (8);

static inline void
dex_object_lock (gpointer data)
{
  g_mutex_lock (&DEX_OBJECT (data)->mutex);
}

static inline void
dex_object_unlock (gpointer data)
{
  g_mutex_unlock (&DEX_OBJECT (data)->mutex);
}

typedef struct _DexObjectClass
{
  GTypeClass parent_class;

  void (*finalize) (DexObject *object);
} DexObjectClass;

DexObject *dex_object_create_instance (GType instance_type);

G_END_DECLS
