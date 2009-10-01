/*
 *  Copyright (C) 2009 Igalia S.L
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

#ifndef DATA_SOURCE_GSTREAMER_H
#define DATA_SOURCE_GSTREAMER_H

#include <glib-object.h>
#include <gst/base/gstbasesrc.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_DATA_SRC            (webkit_data_src_get_type ())
#define WEBKIT_DATA_SRC(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_TYPE_DATA_SRC, WebkitDataSrc))
#define WEBKIT_DATA_SRC_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), WEBKIT_TYPE_DATA_SRC, WebkitDataSrcClass))
#define WEBKIT_IS_DATA_SRC(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_TYPE_DATA_SRC))
#define WEBKIT_IS_DATA_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), WEBKIT_TYPE_DATA_SRC))

typedef struct _WebkitDataSrc        WebkitDataSrc;
typedef struct _WebkitDataSrcClass   WebkitDataSrcClass;


struct _WebkitDataSrc {
    GstBin parent;

    /* explicit pointers to stuff used */
    GstElement* kid;
    GstPad* pad;
    gchar* uri;
};

struct _WebkitDataSrcClass {
    GstBinClass parent_class;
};

GType webkit_data_src_get_type(void);

G_END_DECLS

#endif
