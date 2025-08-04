/*
 * dex-async-pair.h
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

#include "dex-future.h"

G_BEGIN_DECLS

#define DEX_TYPE_ASYNC_PAIR    (dex_async_pair_get_type())
#define DEX_ASYNC_PAIR(obj)    (G_TYPE_CHECK_INSTANCE_CAST(obj, DEX_TYPE_ASYNC_PAIR, DexAsyncPair))
#define DEX_IS_ASYNC_PAIR(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, DEX_TYPE_ASYNC_PAIR))

typedef struct _DexAsyncPair DexAsyncPair;

typedef struct _DexAsyncPairInfo
{
  gpointer  async;
  gpointer  finish;
  GType     return_type;

  /*< private >*/
  gpointer _reserved[13];
} DexAsyncPairInfo;

#ifndef __GI_SCANNER__
G_STATIC_ASSERT (sizeof (DexAsyncPairInfo) == (GLIB_SIZEOF_VOID_P * 16));
#endif

#define DEX_ASYNC_PAIR_INFO(Async, Finish, ReturnType) \
  (DexAsyncPairInfo) {                                 \
    .async = Async,                                    \
    .finish = Finish,                                  \
    .return_type = ReturnType,                         \
  }

#define DEX_ASYNC_PAIR_INFO_BOOLEAN(Async, Finish)     \
  DEX_ASYNC_PAIR_INFO (Async, Finish, G_TYPE_BOOLEAN)

#define DEX_ASYNC_PAIR_INFO_INT(Async, Finish)         \
  DEX_ASYNC_PAIR_INFO (Async, Finish, G_TYPE_INT)
#define DEX_ASYNC_PAIR_INFO_UINT(Async, Finish)        \
  DEX_ASYNC_PAIR_INFO (Async, Finish, G_TYPE_UINT)

#define DEX_ASYNC_PAIR_INFO_INT64(Async, Finish)       \
  DEX_ASYNC_PAIR_INFO (Async, Finish, G_TYPE_INT64)
#define DEX_ASYNC_PAIR_INFO_UINT64(Async, Finish)      \
  DEX_ASYNC_PAIR_INFO (Async, Finish, G_TYPE_UINT64)

#define DEX_ASYNC_PAIR_INFO_LONG(Async, Finish)        \
  DEX_ASYNC_PAIR_INFO (Async, Finish, G_TYPE_LONG)
#define DEX_ASYNC_PAIR_INFO_ULONG(Async, Finish)       \
  DEX_ASYNC_PAIR_INFO (Async, Finish, G_TYPE_ULONG)

#define DEX_ASYNC_PAIR_INFO_STRING(Async, Finish)      \
  DEX_ASYNC_PAIR_INFO (Async, Finish, G_TYPE_STRING)

#define DEX_ASYNC_PAIR_INFO_POINTER(Async, Finish)     \
  DEX_ASYNC_PAIR_INFO (Async, Finish, G_TYPE_POINTER)

#define DEX_ASYNC_PAIR_INFO_OBJECT(Async, Finish)      \
  DEX_ASYNC_PAIR_INFO (Async, Finish, G_TYPE_OBJECT)

#define DEX_ASYNC_PAIR_INFO_BOXED(Async, Finish, Type) \
  DEX_ASYNC_PAIR_INFO (Async, Finish, Type)

DEX_AVAILABLE_IN_ALL
GType         dex_async_pair_get_type              (void) G_GNUC_CONST;
DEX_AVAILABLE_IN_ALL
DexFuture    *dex_async_pair_new                   (gpointer                instance,
                                                    const DexAsyncPairInfo *info)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
GCancellable *dex_async_pair_get_cancellable       (DexAsyncPair           *async_pair);
DEX_AVAILABLE_IN_ALL
void          dex_async_pair_return_object         (DexAsyncPair           *async_pair,
                                                    gpointer                instance);
DEX_AVAILABLE_IN_ALL
void          dex_async_pair_return_error          (DexAsyncPair           *async_pair,
                                                    GError                 *error);
DEX_AVAILABLE_IN_ALL
void          dex_async_pair_return_int64          (DexAsyncPair           *async_pair,
                                                    gint64                  value);
DEX_AVAILABLE_IN_ALL
void          dex_async_pair_return_uint64         (DexAsyncPair           *async_pair,
                                                    guint64                 value);
DEX_AVAILABLE_IN_ALL
void          dex_async_pair_return_boolean        (DexAsyncPair           *async_pair,
                                                    gboolean                value);
DEX_AVAILABLE_IN_ALL
void          dex_async_pair_return_string         (DexAsyncPair           *async_pair,
                                                    char                   *value);
DEX_AVAILABLE_IN_ALL
void          dex_async_pair_return_boxed          (DexAsyncPair           *async_pair,
                                                    GType                   boxed_type,
                                                    gpointer                instance);
DEX_AVAILABLE_IN_ALL
void          dex_async_pair_return_variant        (DexAsyncPair           *async_pair,
                                                    GVariant               *variant);
DEX_AVAILABLE_IN_ALL
void          dex_async_pair_set_cancel_on_discard (DexAsyncPair           *async_pair,
                                                    gboolean                cancel_on_discard);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (DexAsyncPair, dex_unref)

G_END_DECLS
