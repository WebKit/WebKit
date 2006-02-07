/* cairo_output_stream.c: Output stream abstraction
 * 
 * Copyright © 2005 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is cairo_output_stream.c as distributed with the
 *   cairo graphics library.
 *
 * The Initial Developer of the Original Code is Red Hat, Inc.
 *
 * Author(s):
 *	Kristian Høgsberg <krh@redhat.com>
 */

#include <stdio.h>
#include <locale.h>
#include <ctype.h>
#include "cairoint.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif /* _MSC_VER */

struct _cairo_output_stream {
    cairo_write_func_t		write_data;
    void			*closure;
    cairo_bool_t		owns_closure_is_file;
    unsigned long		position;
    cairo_status_t		status;
};

cairo_output_stream_t *
_cairo_output_stream_create (cairo_write_func_t		write_data,
			     void			*closure)
{
    cairo_output_stream_t *stream;

    stream = malloc (sizeof (cairo_output_stream_t));
    if (stream == NULL)
	return NULL;

    stream->write_data = write_data;
    stream->closure = closure;
    stream->owns_closure_is_file = FALSE;
    stream->position = 0;
    stream->status = CAIRO_STATUS_SUCCESS;

    return stream;
}

void
_cairo_output_stream_destroy (cairo_output_stream_t *stream)
{
    if (stream->owns_closure_is_file) {
	FILE *file = stream->closure;
	fflush (file);
	fclose (file);
    }
    free (stream);
}

cairo_status_t
_cairo_output_stream_write (cairo_output_stream_t *stream,
			    const void *data, size_t length)
{
    if (length == 0)
	return CAIRO_STATUS_SUCCESS;

    stream->status = stream->write_data (stream->closure, data, length);
    stream->position += length;

    return stream->status;
}

void
_cairo_output_stream_write_hex_string (cairo_output_stream_t *stream,
				       const char *data,
				       size_t length)
{
    const char hex_chars[] = "0123456789abcdef";
    char buffer[2];
    int i, column;

    for (i = 0, column = 0; i < length; i++, column++) {
	if (column == 38) {
	    _cairo_output_stream_write (stream, "\n", 1);
	    column = 0;
	}
	buffer[0] = hex_chars[(data[i] >> 4) & 0x0f];
	buffer[1] = hex_chars[data[i] & 0x0f];
	_cairo_output_stream_write (stream, buffer, 2);
    }
}

/* Format a double in a locale independent way and trim trailing
 * zeros.  Based on code from Alex Larson <alexl@redhat.com>.
 * http://mail.gnome.org/archives/gtk-devel-list/2001-October/msg00087.html
 */

int
_cairo_dtostr (char *buffer, size_t size, double d)
{
  struct lconv *locale_data;
  const char *decimal_point;
  int decimal_point_len;
  char *p;
  int decimal_len;

  snprintf (buffer, size, "%f", d);
    
  locale_data = localeconv ();
  decimal_point = locale_data->decimal_point;
  decimal_point_len = strlen (decimal_point);
  
  assert (decimal_point_len != 0);
  p = buffer;
			    
  if (*p == '+' || *p == '-')
      p++;

  while (isdigit (*p))
      p++;
					
  if (strncmp (p, decimal_point, decimal_point_len) == 0) {
      *p = '.';
      decimal_len = strlen (p + decimal_point_len);
      memmove (p + 1, p + decimal_point_len, decimal_len);
      p[1 + decimal_len] = 0;

      /* Remove trailing zeros and decimal point if possible. */
      for (p = p + decimal_len; *p == '0'; p--)
	  *p = 0;

      if (*p == '.') {
	  *p = 0;
	  p--;
      }
  }
					        
  return p + 1 - buffer;
}


enum {
    LENGTH_MODIFIER_LONG = 0x100
};

/* Here's a limited reimplementation of printf.  The reason for doing
 * this is primarily to special case handling of doubles.  We want
 * locale independent formatting of doubles and we want to trim
 * trailing zeros.  This is handled by dtostr() above, and the code
 * below handles everything else by calling snprintf() to do the
 * formatting.  This functionality is only for internal use and we
 * only implement the formats we actually use.
 */

cairo_status_t
_cairo_output_stream_vprintf (cairo_output_stream_t *stream,
			      const char *fmt, va_list ap)
{
    char buffer[512];
    char *p;
    const char *f;
    int length_modifier;

    f = fmt;
    p = buffer;
    while (*f != '\0') {
	if (p == buffer + sizeof (buffer)) {
	    _cairo_output_stream_write (stream, buffer, sizeof (buffer));
	    p = buffer;
	}

	if (*f != '%') {
	    *p++ = *f++;
	    continue;
	}

	f++;

	_cairo_output_stream_write (stream, buffer, p - buffer);
	p = buffer;

	length_modifier = 0;
	if (*f == 'l') {
	    length_modifier = LENGTH_MODIFIER_LONG;
	    f++;
	}

	switch (*f | length_modifier) {
	case '%':
	    buffer[0] = *f;
	    buffer[1] = 0;
	    break;
	case 'd':
	    snprintf (buffer, sizeof buffer, "%d", va_arg (ap, int));
	    break;
	case 'd' | LENGTH_MODIFIER_LONG:
	    snprintf (buffer, sizeof buffer, "%ld", va_arg (ap, long int));
	    break;
	case 'u':
	    snprintf (buffer, sizeof buffer, "%u", va_arg (ap, unsigned int));
	    break;
	case 'u' | LENGTH_MODIFIER_LONG:
	    snprintf (buffer, sizeof buffer, "%lu", va_arg (ap, long unsigned int));
	    break;
	case 'o':
	    snprintf (buffer, sizeof buffer, "%o", va_arg (ap, int));
	    break;
	case 's':
	    snprintf (buffer, sizeof buffer, "%s", va_arg (ap, const char *));
	    break;
	case 'f':
	    _cairo_dtostr (buffer, sizeof buffer, va_arg (ap, double));
	    break;
	case 'c':
	    buffer[0] = va_arg (ap, int);
	    buffer[1] = 0;
	    break;
	default:
	    ASSERT_NOT_REACHED;
	}
	p = buffer + strlen (buffer);
	f++;
    }
    
    _cairo_output_stream_write (stream, buffer, p - buffer);

    return stream->status;
}

cairo_status_t
_cairo_output_stream_printf (cairo_output_stream_t *stream,
			     const char *fmt, ...)
{
    va_list ap;
    cairo_status_t status;    

    va_start (ap, fmt);

    status = _cairo_output_stream_vprintf (stream, fmt, ap);

    va_end (ap);

    return status;
}

long
_cairo_output_stream_get_position (cairo_output_stream_t *stream)
{
    return stream->position;
}

cairo_status_t
_cairo_output_stream_get_status (cairo_output_stream_t *stream)
{
    return stream->status;
}


/* Maybe this should be a configure time option, so embedded targets
 * don't have to pull in stdio. */

static cairo_status_t
stdio_write (void *closure, const unsigned char *data, unsigned int length)
{
    FILE *fp = closure;

    if (fwrite (data, 1, length, fp) == length)
	return CAIRO_STATUS_SUCCESS;

    return CAIRO_STATUS_WRITE_ERROR;
}

cairo_output_stream_t *
_cairo_output_stream_create_for_file (const char *filename)
{
    FILE *fp;
    cairo_output_stream_t *stream;

    fp = fopen (filename, "wb");
    if (fp == NULL)
	return NULL;
    
    stream = _cairo_output_stream_create (stdio_write, fp);
    if (stream == NULL)
	fclose (fp);
    stream->owns_closure_is_file = TRUE;

    return stream;
}
