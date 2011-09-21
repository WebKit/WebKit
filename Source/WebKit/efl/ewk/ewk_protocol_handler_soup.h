/*
    Copyright (C) 2011 ProFUSION embedded systems

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef ewk_protocol_handler_soup_h
#define ewk_protocol_handler_soup_h

#include "ewk_protocol_handler.h"

#include <glib-object.h>
#include <glib.h>
#define LIBSOUP_USE_UNSTABLE_REQUEST_API
#include <libsoup/soup-request.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EWK_TYPE_CUSTOM_PROTOCOL_HANDLER            (ewk_custom_protocol_handler_get_type ())
#define EWK_CUSTOM_PROTOCOL_HANDLER(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), EWK_TYPE_CUSTOM_PROTOCOL_HANDLER, EwkCustomProtocolHandler))
#define EWK_CUSTOM_PROTOCOL_HANDLER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EWK_TYPE_CUSTOM_PROTOCOL_HANDLER, EwkCustomProtocolHandlerClass))
#define EWK_IS_CUSTOM_PROTOCOL_HANDLER(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), EWK_TYPE_CUSTOM_PROTOCOL_HANDLER))
#define EWK_IS_CUSTOM_PROTOCOL_HANDLER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), EWK_TYPE_CUSTOM_PROTOCOL_HANDLER))
#define EWK_CUSTOM_PROTOCOL_HANDLER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), EWK_TYPE_CUSTOM_PROTOCOL_HANDLER, EwkCustomProtocolHandlerClass))

typedef struct _EwkProtocolHandlerPrivate EwkProtocolHandlerPrivate;

typedef struct {
    SoupRequest parent;
    EwkProtocolHandlerPrivate* priv;
} EwkCustomProtocolHandler;

typedef struct {
    SoupRequestClass parent;
} EwkCustomProtocolHandlerClass;


GType ewk_custom_protocol_handler_get_type();

Eina_Bool ewk_custom_protocol_handler_soup_set(const char** protocols);

Eina_Bool ewk_custom_protocol_handler_soup_all_unset();

#ifdef __cplusplus
}
#endif

#endif // ewk_protocol_handler_soup_h

