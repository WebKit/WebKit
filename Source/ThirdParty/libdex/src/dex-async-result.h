/* dex-async-result.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gio/gio.h>

#include "dex-future.h"

G_BEGIN_DECLS

#define DEX_TYPE_ASYNC_RESULT (dex_async_result_get_type())

DEX_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (DexAsyncResult, dex_async_result, DEX, ASYNC_RESULT, GObject)

DEX_AVAILABLE_IN_ALL
DexAsyncResult *dex_async_result_new               (gpointer              source_object,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
void            dex_async_result_set_priority      (DexAsyncResult       *async_result,
                                                    int                   priority);
DEX_AVAILABLE_IN_ALL
const char     *dex_async_result_get_name          (DexAsyncResult       *async_result);
DEX_AVAILABLE_IN_ALL
void            dex_async_result_set_name          (DexAsyncResult       *async_result,
                                                    const char           *name);
DEX_AVAILABLE_IN_ALL
void            dex_async_result_set_static_name   (DexAsyncResult       *async_result,
                                                    const char           *name);
DEX_AVAILABLE_IN_ALL
void            dex_async_result_await             (DexAsyncResult       *async_result,
                                                    DexFuture            *future);
DEX_AVAILABLE_IN_ALL
gpointer        dex_async_result_propagate_pointer (DexAsyncResult       *async_result,
                                                    GError              **error)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
gboolean        dex_async_result_propagate_boolean (DexAsyncResult       *async_result,
                                                    GError              **error);
DEX_AVAILABLE_IN_ALL
gssize          dex_async_result_propagate_int     (DexAsyncResult       *async_result,
                                                    GError              **error);
DEX_AVAILABLE_IN_ALL
double          dex_async_result_propagate_double  (DexAsyncResult       *async_result,
                                                    GError              **error);
DEX_AVAILABLE_IN_ALL
DexFuture      *dex_async_result_dup_future        (DexAsyncResult       *async_result)
  G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
