/*
 * Copyright (C) 2011 Igalia S.L.
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
#include "TestMain.h"

#include <gtk/gtk.h>

void beforeAll();
void afterAll();

static void registerGResource(void)
{
    GOwnPtr<char> resourcesPath(g_build_filename(WEBKIT_EXEC_PATH, "resources", "webkit2gtk-tests-resources.gresource", NULL));
    GResource* resource = g_resource_load(resourcesPath.get(), 0);
    g_assert(resource);

    g_resources_register(resource);
    g_resource_unref(resource);
}

int main(int argc, char** argv)
{
    gtk_test_init(&argc, &argv, 0);
    g_setenv("WEBKIT_EXEC_PATH", WEBKIT_EXEC_PATH, FALSE);
    g_setenv("WEBKIT_INJECTED_BUNDLE_PATH", WEBKIT_INJECTED_BUNDLE_PATH, FALSE);
    g_test_bug_base("https://bugs.webkit.org/");

    registerGResource();

    beforeAll();
    int returnValue = g_test_run();
    afterAll();

    return returnValue;
}
