/*
 * Copyright (C) 2009 Jan Michael Alonzo <jmalonzo@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "webkitprivate.h"

#include <wtf/UnusedParam.h>

#include "ApplicationCacheStorage.h"

void webkit_application_cache_set_maximum_size(unsigned long long size)
{
#if ENABLE(OFFLINE_WEB_APPLICATIONS)   
    WebCore::cacheStorage().empty();
    WebCore::cacheStorage().vacuumDatabaseFile();
    WebCore::cacheStorage().setMaximumSize(size);
#else
    UNUSED_PARAM(size);
#endif
}
