/*
 *  Copyright (C) 2014 Igalia S.L.
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

#ifndef WebKitDOMXPathNSResolver_h
#define WebKitDOMXPathNSResolver_h

#include <glib-object.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_XPATH_NS_RESOLVER            (webkit_dom_xpath_ns_resolver_get_type ())
#define WEBKIT_DOM_XPATH_NS_RESOLVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), WEBKIT_DOM_TYPE_XPATH_NS_RESOLVER, WebKitDOMXPathNSResolver))
#define WEBKIT_DOM_XPATH_NS_RESOLVER_CLASS(obj)      (G_TYPE_CHECK_CLASS_CAST ((obj), WEBKIT_DOM_TYPE_XPATH_NS_RESOLVER, WebKitDOMXPathNSResolverIface))
#define WEBKIT_DOM_IS_XPATH_NS_RESOLVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), WEBKIT_DOM_TYPE_XPATH_NS_RESOLVER))
#define WEBKIT_DOM_XPATH_NS_RESOLVER_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), WEBKIT_DOM_TYPE_XPATH_NS_RESOLVER, WebKitDOMXPathNSResolverIface))

struct _WebKitDOMXPathNSResolverIface {
    GTypeInterface gIface;

    /* virtual table */
    gchar *(* lookup_namespace_uri)(WebKitDOMXPathNSResolver *resolver,
                                    const gchar              *prefix);

    void (*_webkitdom_reserved0) (void);
    void (*_webkitdom_reserved1) (void);
    void (*_webkitdom_reserved2) (void);
    void (*_webkitdom_reserved3) (void);
};


WEBKIT_DEPRECATED GType webkit_dom_xpath_ns_resolver_get_type(void) G_GNUC_CONST;

/**
 * webkit_dom_xpath_ns_resolver_lookup_namespace_uri:
 * @resolver: A #WebKitDOMXPathNSResolver
 * @prefix: The prefix to lookup
 *
 * Returns: (transfer full): a #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
WEBKIT_DEPRECATED gchar *webkit_dom_xpath_ns_resolver_lookup_namespace_uri(WebKitDOMXPathNSResolver *resolver,
                                                                    const gchar              *prefix);

G_END_DECLS

#endif /* WebKitDOMXPathNSResolver_h */
