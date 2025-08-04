/*
 * libdex.h
 *
 * Copyright 2022 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define DEX_INSIDE
# include "dex-aio.h"
# include "dex-async-pair.h"
# include "dex-async-result.h"
# include "dex-block.h"
# include "dex-cancellable.h"
# include "dex-channel.h"
# include "dex-delayed.h"
# include "dex-enums.h"
# include "dex-error.h"
# include "dex-fiber.h"
# include "dex-future.h"
# include "dex-future-set.h"
# include "dex-gio.h"
# include "dex-init.h"
# include "dex-main-scheduler.h"
# include "dex-object.h"
# include "dex-platform.h"
# include "dex-promise.h"
# include "dex-scheduler.h"
# include "dex-static-future.h"
# include "dex-thread-pool-scheduler.h"
# include "dex-timeout.h"
#ifdef G_OS_UNIX
# include "dex-unix-signal.h"
#endif
# include "dex-version.h"
# include "dex-version-macros.h"
#undef DEX_INSIDE

G_END_DECLS
