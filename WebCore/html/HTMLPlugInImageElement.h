/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
 *
 */

#ifndef HTMLPlugInImageElement_h
#define HTMLPlugInImageElement_h

#include "HTMLPlugInElement.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

class HTMLImageLoader;

class HTMLPlugInImageElement : public HTMLPlugInElement {
public:
    virtual ~HTMLPlugInImageElement();

    const String& serviceType() const { return m_serviceType; }
    const String& url() const { return m_url; }

protected:
    HTMLPlugInImageElement(const QualifiedName& tagName, Document*);

    bool isImageType();

    OwnPtr<HTMLImageLoader> m_imageLoader;
    String m_serviceType;
    String m_url;
};

} // namespace WebCore

#endif // HTMLPlugInImageElement_h
