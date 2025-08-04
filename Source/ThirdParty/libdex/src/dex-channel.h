/*
 * dex-channel.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#define DEX_TYPE_CHANNEL    (dex_channel_get_type())
#define DEX_CHANNEL(obj)    (G_TYPE_CHECK_INSTANCE_CAST(obj, DEX_TYPE_CHANNEL, DexChannel))
#define DEX_IS_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, DEX_TYPE_CHANNEL))

typedef struct _DexChannel DexChannel;

DEX_AVAILABLE_IN_ALL
GType       dex_channel_get_type          (void) G_GNUC_CONST;
DEX_AVAILABLE_IN_ALL
DexChannel *dex_channel_new               (guint       capacity)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture  *dex_channel_send              (DexChannel *channel,
                                           DexFuture  *future)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture  *dex_channel_receive           (DexChannel *channel)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture  *dex_channel_receive_all       (DexChannel *channel)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
void        dex_channel_close_send        (DexChannel *channel);
DEX_AVAILABLE_IN_ALL
void        dex_channel_close_receive     (DexChannel *channel);
DEX_AVAILABLE_IN_ALL
gboolean    dex_channel_can_send          (DexChannel *channel);
DEX_AVAILABLE_IN_ALL
gboolean    dex_channel_can_receive       (DexChannel *channel);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (DexChannel, dex_unref)

G_END_DECLS
