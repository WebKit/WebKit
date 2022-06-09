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

#pragma once

#include "WebKitDOMXPathNSResolver.h"
#include <WebCore/XPathNSResolver.h>
#include <wtf/glib/GRefPtr.h>

namespace WebKit {

class GObjectXPathNSResolver : public WebCore::XPathNSResolver {
public:

    static Ref<GObjectXPathNSResolver> create(WebKitDOMXPathNSResolver* resolver)
    {
        return adoptRef(*new GObjectXPathNSResolver(resolver));
    }

    virtual ~GObjectXPathNSResolver();
    String lookupNamespaceURI(const String& prefix) override;

private:
    GObjectXPathNSResolver(WebKitDOMXPathNSResolver* resolver)
        : m_resolver(resolver)
    {
    }

    GRefPtr<WebKitDOMXPathNSResolver> m_resolver;
};

} // namespace WebKit
