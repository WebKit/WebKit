/*
 * dex-semaphore-private.h
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

#include "dex-future.h"

G_BEGIN_DECLS

#define DEX_TYPE_SEMAPHORE    (dex_semaphore_get_type())
#define DEX_SEMAPHORE(obj)    (G_TYPE_CHECK_INSTANCE_CAST(obj, DEX_TYPE_SEMAPHORE, DexSemaphore))
#define DEX_IS_SEMAPHORE(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, DEX_TYPE_SEMAPHORE))

typedef struct _DexSemaphore DexSemaphore;

GType         dex_semaphore_get_type   (void) G_GNUC_CONST;
DexSemaphore *dex_semaphore_new        (void);
void          dex_semaphore_post       (DexSemaphore *semaphore);
void          dex_semaphore_post_many  (DexSemaphore *semaphore,
                                        guint         count);
DexFuture    *dex_semaphore_wait       (DexSemaphore *semaphore);
void          dex_semaphore_close      (DexSemaphore *semaphore);

G_END_DECLS
