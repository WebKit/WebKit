/* dex-aio.h
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

#include "dex-future.h"

G_BEGIN_DECLS

typedef struct _DexAioContext DexAioContext;

DEX_AVAILABLE_IN_ALL
DexFuture *dex_aio_read  (DexAioContext *aio_context,
                          int            fd,
                          gpointer       buffer,
                          gsize          count,
                          goffset        offset)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture *dex_aio_write (DexAioContext *aio_context,
                          int            fd,
                          gconstpointer  buffer,
                          gsize          count,
                          goffset        offset)
  G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
