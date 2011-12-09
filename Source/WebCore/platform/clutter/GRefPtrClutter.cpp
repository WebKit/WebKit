/*
 *  Copyright (C) 2011 Collabora Ltd.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "config.h"
#include "GRefPtrClutter.h"

#include <clutter/clutter.h>

namespace WTF {

template <> GRefPtr<ClutterActor> adoptGRef(ClutterActor* ptr)
{
    if (g_object_is_floating(ptr))
        g_object_ref_sink(ptr);

    return GRefPtr<ClutterActor>(ptr, GRefPtrAdopt);
}

template <> ClutterActor* refGPtr<ClutterActor>(ClutterActor* ptr)
{
    if (ptr) {
        if (g_object_is_floating(ptr))
            g_object_ref_sink(ptr);

        g_object_ref(ptr);
    }

    return ptr;
}

template <> void derefGPtr<ClutterActor>(ClutterActor* ptr)
{
    if (ptr)
        g_object_unref(ptr);
}

}
