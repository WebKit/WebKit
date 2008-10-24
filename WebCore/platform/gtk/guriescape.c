/*
 * Copyright (C) 2008 Collabora, Ltd.
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1997-2000 The GLib Team
 * Copyright (C) 2006-2007 Red Hat, Inc.
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
#include "guriescape.h"

#include <string.h>

#if !PLATFORM(WIN_OS) && !GLIB_CHECK_VERSION(2,16,0)

/* is_valid, gunichar_ok and g_string_append_uri_escaped were copied for glib/gstring.c
 * in the glib package.
 *
 * Original copyright:
 *   Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 *   Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 *   file for a list of people on the GLib Team.  See the ChangeLog
 *   files for a list of changes.  These files are distributed with
 *   GLib at ftp://ftp.gtk.org/pub/gtk/. 
 *
 * Please don't change the indentation so it's easier to update these functions
 * if they are changed in glib.
 */
static gboolean
is_valid (char c, const char *reserved_chars_allowed)
{
  if (g_ascii_isalnum (c) ||
      c == '-' ||
      c == '.' ||
      c == '_' ||
      c == '~')
    return TRUE;

  if (reserved_chars_allowed &&
      strchr (reserved_chars_allowed, c) != NULL)
    return TRUE;
  
  return FALSE;
}

static gboolean 
gunichar_ok (gunichar c)
{
  return
    (c != (gunichar) -2) &&
    (c != (gunichar) -1);
}

static GString *
_webcore_g_string_append_uri_escaped (GString *string,
                             const char *unescaped,
                             const char *reserved_chars_allowed,
                             gboolean allow_utf8)
{
  unsigned char c;
  const char *end;
  static const gchar hex[16] = "0123456789ABCDEF";

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (unescaped != NULL, NULL);

  end = unescaped + strlen (unescaped);
  
  while ((c = *unescaped) != 0)
    {
      if (c >= 0x80 && allow_utf8 &&
          gunichar_ok (g_utf8_get_char_validated (unescaped, end - unescaped)))
        {
          int len = g_utf8_skip [c];
          g_string_append_len (string, unescaped, len);
          unescaped += len;
        }
      else if (is_valid (c, reserved_chars_allowed))
        {
          g_string_append_c (string, c);
          unescaped++;
        }
      else
        {
          g_string_append_c (string, '%');
          g_string_append_c (string, hex[((guchar)c) >> 4]);
          g_string_append_c (string, hex[((guchar)c) & 0xf]);
          unescaped++;
        }
    }

  return string;
}

/* g_uri_escape_string, unescape_character, g_uri_unescape_segment and
 * g_uri_unescape_string were copied for glib/gurifuncs.c in the glib package
 * and prefixed with _webcore (if necessary) to avoid exporting a symbol with
 * the "g_" prefix.
 *
 * Original copyright:
 *   Copyright (C) 2006-2007 Red Hat, Inc.
 *   Author: Alexander Larsson <alexl@redhat.com>
 *
 * Please don't change the indentation so it's easier to update this function
 * if it's changed in glib.
 */
char *
_webcore_g_uri_escape_string (const char *unescaped,
                     const char  *reserved_chars_allowed,
                     gboolean     allow_utf8)
{
  GString *s;

  g_return_val_if_fail (unescaped != NULL, NULL);

  s = g_string_sized_new (strlen (unescaped) + 10);
  
  _webcore_g_string_append_uri_escaped (s, unescaped, reserved_chars_allowed, allow_utf8);
  
  return g_string_free (s, FALSE);
}

static int
unescape_character (const char *scanner)
{
  int first_digit;
  int second_digit;
  
  first_digit = g_ascii_xdigit_value (*scanner++);
  if (first_digit < 0)
    return -1;

  second_digit = g_ascii_xdigit_value (*scanner++);
  if (second_digit < 0)
    return -1;

  return (first_digit << 4) | second_digit;
}



static char *
_webcore_g_uri_unescape_segment (const char *escaped_string,
      const char *escaped_string_end,
      const char *illegal_characters)
{
  const char *in;
  char *out, *result;
  gint character;
  
  if (escaped_string == NULL)
    return NULL;
  
  if (escaped_string_end == NULL)
    escaped_string_end = escaped_string + strlen (escaped_string);
  
  result = g_malloc (escaped_string_end - escaped_string + 1);
  
  out = result;
  for (in = escaped_string; in < escaped_string_end; in++)
    {
      character = *in;
      
      if (*in == '%')
  {
    in++;
    
    if (escaped_string_end - in < 2)
      {
        /* Invalid escaped char (to short) */
        g_free (result);
        return NULL;
      }
    
    character = unescape_character (in);
    
    /* Check for an illegal character. We consider '\0' illegal here. */
    if (character <= 0 ||
        (illegal_characters != NULL &&
         strchr (illegal_characters, (char)character) != NULL))
      {
        g_free (result);
        return NULL;
      }
    
    in++; /* The other char will be eaten in the loop header */
  }
      *out++ = (char)character;
    }
  
  *out = '\0';
  
  return result;
}


char *
_webcore_g_uri_unescape_string (const char *escaped_string,
           const char *illegal_characters)
{
  return _webcore_g_uri_unescape_segment (escaped_string, NULL, illegal_characters);
}

#endif /* #if !PLATFORM(WIN_OS) && !GLIB_CHECK_VERSION(2,16,0) */
