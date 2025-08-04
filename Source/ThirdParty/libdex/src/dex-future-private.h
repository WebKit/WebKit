/*
 * dex-future-private.h
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

#pragma once

#ifndef PACKAGE_VERSION
# error "config.h must be included before dex-future-private.h"
#endif

#include "dex-future.h"
#include "dex-object-private.h"

G_BEGIN_DECLS

#define DEX_FUTURE_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST(klass, DEX_TYPE_FUTURE, DexFutureClass))
#define DEX_FUTURE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS(obj, DEX_TYPE_FUTURE, DexFutureClass))

typedef struct _DexFuture
{
  DexObject parent_instance;
  GValue resolved;
  GError *rejected;
  GQueue chained;
  const char *name;
  DexFutureStatus status : 2;
} DexFuture;

typedef struct _DexFutureClass
{
  DexObjectClass parent_class;

  gboolean (*propagate) (DexFuture *future,
                         DexFuture *completed);
  void     (*discard)   (DexFuture *future);
} DexFutureClass;

void          dex_future_chain         (DexFuture     *future,
                                        DexFuture     *chained);
void          dex_future_complete      (DexFuture     *future,
                                        const GValue  *value,
                                        GError        *error);
void          dex_future_complete_from (DexFuture     *future,
                                        DexFuture     *completed);
void          dex_future_discard       (DexFuture     *future,
                                        DexFuture     *chained);
const GValue *dex_await_borrowed       (DexFuture     *future,
                                        GError       **error);

G_END_DECLS
