/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "TestController.h"
#include <Ecore.h>
#include <Ecore_Evas.h>
#include <Edje.h>
#include <glib-object.h>
#include <glib.h>

#ifdef HAVE_ECORE_X
#include <Ecore_X.h>
#endif

int main(int argc, char** argv)
{
    g_type_init();

    if (!ecore_evas_init())
        return 1;

    if (!edje_init()) {
        ecore_evas_shutdown();
        return 1;
    }

#ifdef HAVE_ECORE_X
    if (!ecore_x_init(0)) {
        ecore_evas_shutdown();
        edje_shutdown();
        return 1;
    }
#endif

    // Prefer the not installed web and plugin processes.
    WTR::TestController controller(argc, const_cast<const char**>(argv));

#ifdef HAVE_ECORE_X
    ecore_x_shutdown();
#endif

    edje_shutdown();
    ecore_evas_shutdown();

    return 0;
}

