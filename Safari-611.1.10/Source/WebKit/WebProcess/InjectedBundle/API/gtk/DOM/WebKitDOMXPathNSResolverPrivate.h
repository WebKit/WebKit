/*
 *  This file is part of the WebKit open source project.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef WebKitDOMXPathNSResolverPrivate_h
#define WebKitDOMXPathNSResolverPrivate_h

#include <WebCore/XPathNSResolver.h>
#include <webkitdom/WebKitDOMXPathNSResolver.h>

#define WEBKIT_DOM_TYPE_NATIVE_XPATH_NS_RESOLVER            (webkit_dom_native_xpath_ns_resolver_get_type())
#define WEBKIT_DOM_NATIVE_XPATH_NS_RESOLVER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_NATIVE_XPATH_NS_RESOLVER, WebKitDOMNativeXPathNSResolver))
#define WEBKIT_DOM_NATIVE_XPATH_NS_RESOLVER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_NATIVE_XPATH_NS_RESOLVER, WebKitDOMNativeXPathNSResolverClass)
#define WEBKIT_DOM_IS_NATIVE_XPATH_NS_RESOLVER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_NATIVE_XPATH_NS_RESOLVER))
#define WEBKIT_DOM_IS_NATIVE_XPATH_NS_RESOLVER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_NATIVE_XPATH_NS_RESOLVER))
#define WEBKIT_DOM_NATIVE_XPATH_NS_RESOLVER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_NATIVE_XPATH_NS_RESOLVER, WebKitDOMNativeXPathNSResolverClass))

typedef struct _WebKitDOMNativeXPathNSResolver WebKitDOMNativeXPathNSResolver;
typedef struct _WebKitDOMNativeXPathNSResolverClass WebKitDOMNativeXPathNSResolverClass;

namespace WebKit {
RefPtr<WebCore::XPathNSResolver> core(WebKitDOMXPathNSResolver*);
WebKitDOMXPathNSResolver* kit(WebCore::XPathNSResolver*);
WebCore::XPathNSResolver* core(WebKitDOMNativeXPathNSResolver*);
} // namespace WebKit

#endif /* WebKitDOMXPathNSResolverPrivate_h */
