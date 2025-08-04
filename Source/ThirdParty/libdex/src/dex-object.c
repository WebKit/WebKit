/*
 * dex-object.c
 *
 * Copyright 2022 Christian Hergert <chergert@gnome.org>
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

#include "config.h"

#include <stdatomic.h>

#include <gobject/gvaluecollector.h>

#include "dex-object-private.h"
#include "dex-profiler.h"

/**
 * DexObject: (ref-func dex_ref) (unref-func dex_unref)
 *   (set-value-func dex_value_set_object)
 *   (get-value-func dex_value_get_object)
 *
 * `DexObject` is the basic building block of types defined within
 * libdex. Futures, Schedulers, and Channels all inherit from DexObject
 * which provides features like thread-safe weak pointers and memory
 * management operations.
 *
 * Objects that are integrating with GIO instead inherit from their
 * natural type in GIO.
 */

static GType dex_object_type = G_TYPE_INVALID;

#undef DEX_TYPE_OBJECT
#define DEX_TYPE_OBJECT dex_object_type

static void
dex_object_finalize (DexObject *object)
{
  g_assert (object != NULL);
  g_assert (object->ref_count == 0);

#ifdef HAVE_SYSPROF
  DEX_PROFILER_MARK (0, DEX_OBJECT_TYPE_NAME (object), "dex_object_finalize()");
  DEX_PROFILER_MARK (SYSPROF_CAPTURE_CURRENT_TIME - object->ctime, DEX_OBJECT_TYPE_NAME (object), "lifetime");
#endif

  g_type_free_instance ((GTypeInstance *)object);
}

/**
 * dex_ref: (method)
 * @object: (type DexObject): the object to reference
 *
 * Acquires a reference on the given object, and increases its reference count by one.
 *
 * Returns: (transfer full) (type DexObject): the object with its reference count increased
 */
gpointer
dex_ref (gpointer object)
{
  DexObject *self = object;
  atomic_fetch_add_explicit (&self->ref_count, 1, memory_order_relaxed);
  return object;
}

/**
 * dex_unref: (method)
 * @object: (type DexObject) (transfer full): the object to unreference
 *
 * Releases a reference on the given object, and decreases its reference count by one.
 *
 * If it was the last reference, the resources associated to the instance are freed.
 */
void
dex_unref (gpointer object)
{
  DexObject *obj = object;
  DexObjectClass *object_class;
  DexWeakRef *weak_refs;
  guint watermark;

  g_return_if_fail (object != NULL);
  g_return_if_fail (DEX_IS_OBJECT (object));

  /* Fetch a watermark before we decrement so that we can be
   * sure that if we reached zero, that anything that extended
   * the life of the mem_block will be responsible to free
   * the object in the end.
   */
  watermark = g_atomic_int_get (&obj->weak_refs_watermark);

  /* If we decrement and it's not zero, then there is nothing
   * for this thread to do. Fast path.
   */
  if G_LIKELY (atomic_fetch_sub_explicit (&obj->ref_count, 1, memory_order_release) != 1)
    return;

  atomic_thread_fence (memory_order_acquire);

  object_class = DEX_OBJECT_GET_CLASS (object);

  /* We reached zero. We need to go through our weak references and
   * acquire each of their mutexes so that we can be sure none of them
   * have raced against us to increment the value beyond 0. If they did,
   * then we can bail early and ignore any request to finalize.
   *
   * If the watermark is beyond ours, then we know that the weak-ref
   * extended liveness beyond this call and we are not responsible
   * for finalizing this object.
   */
  dex_object_lock (object);
  for (DexWeakRef *wr = obj->weak_refs; wr; wr = wr->next)
    g_mutex_lock (&wr->mutex);

  /* If we increased our reference count on another thread by extending the
   * lifetime of the mem_block using a weak_ref-to-full_ref, then we don't need
   * to do anything here. Just bail and move along allowing a future unref to
   * finalize when it once again approaches zero.
   */
  if (g_atomic_int_get (&obj->ref_count) > 0 ||
      g_atomic_int_get (&obj->weak_refs_watermark) != watermark)
    {
      for (DexWeakRef *wr = obj->weak_refs; wr; wr = wr->next)
        g_mutex_unlock (&wr->mutex);
      dex_object_unlock (object);
      return;
    }

  weak_refs = g_steal_pointer (&obj->weak_refs);

  /* So we have locks on everything we can, we still are at zero, so that
   * means we need to zero out all our weak refs and then finalize the
   * object using the provided @clear_func.
   */
  while (weak_refs != NULL)
    {
      DexWeakRef *wr = weak_refs;

      weak_refs = weak_refs->next;

      wr->prev = NULL;
      wr->next = NULL;
      wr->mem_block = NULL;

      g_mutex_unlock (&wr->mutex);
    }

  dex_object_unlock (object);

  /* If we did not create an immortal ref, then we are safe to finalize */
  if (g_atomic_int_get (&obj->ref_count) == 0)
    object_class->finalize (object);
}

static void
dex_object_class_init (DexObjectClass *klass)
{
  klass->finalize = dex_object_finalize;
}

static void
dex_object_init (DexObject      *self,
                 DexObjectClass *object_class)
{
#ifdef HAVE_SYSPROF
  self->ctime = SYSPROF_CAPTURE_CURRENT_TIME;
  DEX_PROFILER_MARK (0,
                     g_type_name (G_TYPE_FROM_CLASS (object_class)),
                     "dex_object_init()");
#endif

  self->ref_count = 1;
  g_mutex_init (&self->mutex);
  self->weak_refs_watermark = 1;
}

static void
dex_object_add_weak (gpointer    mem_block,
                      DexWeakRef *weak_ref)
{
  DexObject *object = mem_block;

  g_assert (object != NULL);
  g_assert (weak_ref != NULL);
  g_assert (weak_ref->prev == NULL);
  g_assert (weak_ref->next == NULL);
  g_assert (weak_ref->mem_block == mem_block);

  /* Must own a full ref to acquire a weak ref */
  g_return_if_fail (object->ref_count > 0);

  dex_object_lock (object);
  weak_ref->prev = NULL;
  weak_ref->next = object->weak_refs;
  if (object->weak_refs != NULL)
    object->weak_refs->prev = weak_ref;
  object->weak_refs = weak_ref;
  dex_object_unlock (object);
}

static void
dex_object_remove_weak (gpointer    mem_block,
                         DexWeakRef *weak_ref)
{
  DexObject *object = mem_block;

  g_assert (object != NULL);
  g_assert (weak_ref != NULL);

  /* Must own a full ref to release a weak ref */
  g_return_if_fail (object->ref_count > 0);

  dex_object_lock (object);

  if (weak_ref->prev != NULL)
    weak_ref->prev->next = weak_ref->next;

  if (weak_ref->next != NULL)
    weak_ref->next->prev = weak_ref->prev;

  if (object->weak_refs == weak_ref)
    object->weak_refs = weak_ref->next;

  g_assert (object->weak_refs == NULL ||
            object->weak_refs->prev == NULL);

  weak_ref->next = NULL;
  weak_ref->prev = NULL;
  weak_ref->mem_block = NULL;

  dex_object_unlock (object);
}

/**
 * dex_weak_ref_init: (skip)
 * @weak_ref: (out caller-allocates): uninitialized memory to store a weak ref
 * @mem_block: (nullable): the mem_block weak reference
 *
 * Creates a new weak reference to @mem_block.
 *
 * @mem_block must be a type that is created with dex_object_alloc0(),
 * dex_object_new0(), or similar; otherwise %NULL.
 *
 * It is an error to create a new weak reference after @mem_block has
 * started disposing which in practice means you must own a full reference
 * to create a weak reference.
 */
void
dex_weak_ref_init (DexWeakRef *weak_ref,
                   gpointer    mem_block)
{
  g_return_if_fail (weak_ref != NULL);
  g_return_if_fail (!mem_block || DEX_IS_OBJECT (mem_block));
  g_return_if_fail (!mem_block || DEX_OBJECT (mem_block)->ref_count > 0);

  memset (weak_ref, 0, sizeof *weak_ref);
  g_mutex_init (&weak_ref->mutex);

  if (mem_block)
    dex_weak_ref_set (weak_ref, mem_block);
}

static inline gpointer
dex_weak_ref_get_locked (DexWeakRef *weak_ref)
{
  if (weak_ref->mem_block != NULL)
    {
      guint watermark;

      /* We have a pointer to our mem_block still. That means either the
       * object has a reference count greater-than zero, or we are running
       * against the finalizer (which must acquire weak_ref->mutex before it
       * can attempt to run finalizers).
       */
      DexObject *object = weak_ref->mem_block;

      /* Increment the watermark so that any calls to dex_unref() racing
       * against us can detect that we extended liveness. If we have raced
       * G_MAXUINT32 times, then something nefarious is going on and we can
       * just make the object immortal.
       *
       * Otherwise, just add a single reference to own the object.
       */
      watermark = g_atomic_int_add (&object->weak_refs_watermark, 1);
      atomic_fetch_add_explicit (&object->ref_count,
                                 1 + (watermark == G_MAXUINT32),
                                 memory_order_relaxed);

#ifdef HAVE_SYSPROF
      {
        char *message = g_strdup_printf ("%s@%p converted to full",
                                         DEX_OBJECT_TYPE_NAME (weak_ref->mem_block),
                                         weak_ref->mem_block);
        DEX_PROFILER_MARK (0, "DexWeakRef", message);
        g_free (message);
      }
#endif

      return weak_ref->mem_block;
    }

  return NULL;
}

/**
 * dex_weak_ref_get: (skip)
 * @weak_ref: a #DexWeakRef
 *
 * Converts a weak ref into a full reference.
 *
 * This attempts to convert the #DexWeakRef created with
 * dex_weak_ref_init() into a full reference.
 *
 * If the mem_block pointed to by @weak_ref has already been released, or
 * is racing against disposal, %NULL is returned.
 *
 * Returns: (transfer full) (nullable): the mem_block or %NULL
 */
gpointer
dex_weak_ref_get (DexWeakRef *weak_ref)
{
  gpointer ret;

  g_return_val_if_fail (weak_ref != NULL, NULL);

  g_mutex_lock (&weak_ref->mutex);
  ret = dex_weak_ref_get_locked (weak_ref);
  g_mutex_unlock (&weak_ref->mutex);

  return ret;
}

/**
 * dex_weak_ref_clear: (skip)
 * @weak_ref: a #DexWeakRef
 *
 * Clears a #DexWeakRef that was previous registered with a mem_block
 * using dex_weak_ref_init().
 *
 * It is an error to call this method while other threads are accessing
 * the #DexWeakRef.
 */
void
dex_weak_ref_clear (DexWeakRef *weak_ref)
{
  gpointer mem_block;

  g_return_if_fail (weak_ref != NULL);

  /* To detach a weak ref, you MUST own a full reference first. If we
   * fail to acquire a full reference, then our weak ref has been
   * abandoned by the mem_block and we are free to clean it up by
   * clearing our mutex.
   */

  mem_block = dex_weak_ref_get (weak_ref);

  if (mem_block != NULL)
    {
      dex_object_remove_weak (mem_block, weak_ref);
      dex_clear (&mem_block);
    }

  /* These should be NULL because they've been removed above, or were
   * already removed during finalization.
   */
  g_assert (weak_ref->prev == NULL);
  g_assert (weak_ref->next == NULL);
  g_assert (weak_ref->mem_block == NULL);

  g_mutex_clear (&weak_ref->mutex);
}

/**
 * dex_weak_ref_set: (skip)
 * @weak_ref: a #DexWeakRef
 * @mem_block: (nullable): the mem_block or %NULL
 *
 * Sets a #DexWeakRef to @mem_block.
 *
 * @mem_block must be a type allocated with dex_object_alloc0() or
 * equivalent allocator.
 *
 * It is an error to call this method without a full reference to
 * @mem_block.
 */
void
dex_weak_ref_set (DexWeakRef *weak_ref,
                  gpointer    mem_block)
{
  gpointer old_mem_block;

  g_return_if_fail (weak_ref != NULL);
  g_return_if_fail (!mem_block || DEX_IS_OBJECT (mem_block));
  g_return_if_fail (!mem_block || DEX_OBJECT (mem_block)->ref_count > 0);

  g_mutex_lock (&weak_ref->mutex);

  old_mem_block = dex_weak_ref_get_locked (weak_ref);

  if (old_mem_block != mem_block)
    {
      if (old_mem_block != NULL)
        dex_object_remove_weak (old_mem_block, weak_ref);
      weak_ref->mem_block = mem_block;
      if (mem_block != NULL)
        dex_object_add_weak (mem_block, weak_ref);
    }

  g_mutex_unlock (&weak_ref->mutex);

  dex_clear (&old_mem_block);
}

static void
value_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_free_value (GValue *value)
{
  dex_clear (&value->data[0].v_pointer);
}

static void
value_copy_value (const GValue *src,
                  GValue       *dst)
{
  if (src->data[0].v_pointer != NULL)
    dst->data[0].v_pointer = dex_ref (src->data[0].v_pointer);
  else
    dst->data[0].v_pointer = NULL;
}

static gpointer
value_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

static char *
value_collect_value (GValue      *value,
                     guint        n_collect_values,
                     GTypeCValue *collect_values,
                     guint        collect_flags)
{
  DexObject *object = collect_values[0].v_pointer;

  if (object == NULL)
    {
      value->data[0].v_pointer = NULL;
      return NULL;
    }

  if (object->parent_instance.g_class == NULL)
    return g_strconcat ("invalid unclassed DexObject pointer for value type '",
                        G_VALUE_TYPE_NAME (value),
                        "'",
                        NULL);

  value->data[0].v_pointer = dex_ref (object);

  return NULL;
}

static char *
value_lcopy_value (const GValue *value,
                   guint         n_collect_values,
                   GTypeCValue  *collect_values,
                   guint         collect_flags)
{
  const DexObject **object_p = collect_values[0].v_pointer;

  if G_UNLIKELY (object_p == NULL)
    return g_strconcat ("value location for '",
                        G_VALUE_TYPE_NAME (value),
                        "' passed as NULL",
                        NULL);

  if (value->data[0].v_pointer == NULL)
    *object_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *object_p = value->data[0].v_pointer;
  else
    *object_p = dex_ref (value->data[0].v_pointer);

  return NULL;
}

GType
dex_object_get_type (void)
{
  if (g_once_init_enter (&dex_object_type))
    {
      GType gtype =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     g_intern_static_string ("DexObject"),
                                     &(const GTypeInfo) {
                                       sizeof (DexObjectClass),
                                       (GBaseInitFunc) NULL,
                                       (GBaseFinalizeFunc) NULL,
                                       (GClassInitFunc) dex_object_class_init,
                                       (GClassFinalizeFunc) NULL,
                                       NULL,

                                       sizeof (DexObject),
                                       0,
                                       (GInstanceInitFunc) dex_object_init,

                                       /* GValue */
                                       &(const GTypeValueTable) {
                                         value_init,
                                         value_free_value,
                                         value_copy_value,
                                         value_peek_pointer,
                                         "p",
                                         value_collect_value,
                                         "p",
                                         value_lcopy_value,
                                       },
                                     },
                                     &(const GTypeFundamentalInfo) {
                                       (G_TYPE_FLAG_INSTANTIATABLE |
                                        G_TYPE_FLAG_CLASSED |
                                        G_TYPE_FLAG_DERIVABLE |
                                        G_TYPE_FLAG_DEEP_DERIVABLE),
                                     },
                                     G_TYPE_FLAG_ABSTRACT);
      g_assert (gtype != G_TYPE_INVALID);
      g_once_init_leave (&dex_object_type, gtype);
    }

  return dex_object_type;
}

DexObject *
dex_object_create_instance (GType instance_type)
{
  return (DexObject *)(gpointer)g_type_create_instance (instance_type);
}

/**
 * dex_value_get_object:
 * @value: a `GValue` initialized with type `DEX_TYPE_OBJECT`
 *
 * Retrieves the `DexObject` stored inside the given `value`.
 *
 * Returns: (transfer none) (nullable): a `DexObject`
 *
 * Since: 0.4
 */
DexObject *
dex_value_get_object (const GValue *value)
{
  g_return_val_if_fail (G_VALUE_HOLDS (value, DEX_TYPE_OBJECT), NULL);

  return value->data[0].v_pointer;
}

/**
 * dex_value_set_object:
 * @value: a [struct@GObject.Value] initialized with type `DEX_TYPE_OBJECT`
 * @object: (nullable): a `DexObject` or %NULL
 *
 * Stores the given `DexObject` inside `value`.
 *
 * The [struct@GObject.Value] will acquire a reference to the `object`.
 *
 * Since: 0.4
 */
void
dex_value_set_object (GValue    *value,
                      DexObject *object)
{
  if (object != NULL)
    dex_ref (object);

  dex_value_take_object (value, object);
}

/**
 * dex_value_take_object:
 * @value: a [struct@GObject.Value] initialized with type `DEX_TYPE_OBJECT`
 * @object: (transfer full) (nullable): a `DexObject`
 *
 * Stores the given `DexObject` inside `value`.
 *
 * This function transfers the ownership of the `object` to the `GValue`.
 *
 * Since: 0.4
 */
void
dex_value_take_object (GValue    *value,
                       DexObject *object)
{
  DexObject *old_object;

  g_return_if_fail (G_VALUE_HOLDS (value, DEX_TYPE_OBJECT));

  old_object = value->data[0].v_pointer;

  if (object != NULL)
    {
      g_return_if_fail (DEX_IS_OBJECT (object));

      value->data[0].v_pointer = object;
    }
  else
    {
      value->data[0].v_pointer = NULL;
    }

  if (old_object != NULL)
    dex_unref (old_object);
}
