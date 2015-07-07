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

#ifndef GObjectXPathNSResolver_h
#define GObjectXPathNSResolver_h

#include "WebKitDOMXPathNSResolver.h"
#include "XPathNSResolver.h"
#include <wtf/PassRefPtr.h>
#include <wtf/gobject/GRefPtr.h>

namespace WebCore {

class GObjectXPathNSResolver : public XPathNSResolver {
public:

    static PassRefPtr<GObjectXPathNSResolver> create(WebKitDOMXPathNSResolver* resolver)
    {
        return adoptRef(new GObjectXPathNSResolver(resolver));
    }

    virtual ~GObjectXPathNSResolver();
    virtual String lookupNamespaceURI(const String& prefix);

private:
    GObjectXPathNSResolver(WebKitDOMXPathNSResolver* resolver)
        : m_resolver(resolver)
    {
    }

    GRefPtr<WebKitDOMXPathNSResolver> m_resolver;
};

} // namespace WebCore

#endif // GObjectXPathNSResolver_h
