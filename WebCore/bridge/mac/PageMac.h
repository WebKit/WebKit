// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef PAGE_MAC_H
#define PAGE_MAC_H

#include "Page.h"

namespace WebCore {

    class PageMac : public Page {
    public:
        PageMac(WebCorePageBridge* b) : m_bridge(b) { }
        WebCorePageBridge* bridge() const { return m_bridge; }
    private:
        WebCorePageBridge* m_bridge;
    };

    inline PageMac* Mac(Page* page) { return static_cast<PageMac*>(page); }
    inline const PageMac* Mac(const Page* page) { return static_cast<const PageMac*>(page); }

} // namespace WebCore
    
#endif // PAGE_H
