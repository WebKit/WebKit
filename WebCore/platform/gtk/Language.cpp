/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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
#include "Language.h"

#include "CString.h"
#include "PlatformString.h"

#include <gtk/gtk.h>
#include <pango/pango.h>

#if !defined(PANGO_VERSION_CHECK)
// PANGO_VERSION_CHECK() and pango_language_get_default() appeared in 1.5.2
#include <locale.h>

static gchar *
_pango_get_lc_ctype (void)
{
  return g_strdup (setlocale (LC_CTYPE, NULL));
}

static PangoLanguage *
pango_language_get_default (void)
{
  static PangoLanguage *result = NULL;
  if (G_UNLIKELY (!result))
    {
      gchar *lang = _pango_get_lc_ctype ();
      result = pango_language_from_string (lang);
      g_free (lang);
    }
  return result;
}
#endif

namespace WebCore {

String defaultLanguage()
{
    return pango_language_to_string(gtk_get_default_language());
}

}
