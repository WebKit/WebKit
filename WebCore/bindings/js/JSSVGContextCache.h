/*
    Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.

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

#ifndef JSSVGContextCache_h
#define JSSVGContextCache_h

#if ENABLE(SVG)
#include "SVGElement.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

class DOMObject;

class JSSVGContextCache : public Noncopyable {
public:
    typedef HashMap<DOMObject*, SVGElement*> WrapperMap;

    static WrapperMap& wrapperMap()
    {
        DEFINE_STATIC_LOCAL(WrapperMap, s_wrapperMap, ());
        return s_wrapperMap;
    }

    static void addWrapper(DOMObject* wrapper, SVGElement* context)
    {
        ASSERT(wrapper);
        ASSERT(context);

        pair<WrapperMap::iterator, bool> result = wrapperMap().add(wrapper, context);
        if (result.second) {
            WrapperMap::iterator& it = result.first;
            ASSERT_UNUSED(it, it->second == context);
        }
    }

    static void forgetWrapper(DOMObject* wrapper)
    {
        ASSERT(wrapper);

        WrapperMap& map = wrapperMap();
        WrapperMap::iterator it = map.find(wrapper);
        if (it == map.end())
            return;

        map.remove(it);
    }

    static void propagateSVGDOMChange(DOMObject* wrapper, const QualifiedName& attributeName)
    {
        WrapperMap& map = wrapperMap();
        WrapperMap::iterator it = map.find(wrapper);
        if (it == map.end())
            return;

        SVGElement* context = it->second;
        ASSERT(context);

        context->svgAttributeChanged(attributeName);
    }

    static SVGElement* svgContextForDOMObject(DOMObject* wrapper)
    {
        ASSERT(wrapper);

        WrapperMap& map = wrapperMap();
        WrapperMap::iterator it = map.find(wrapper);
        if (it == map.end())
            return 0;

        SVGElement* context = it->second;
        ASSERT(context);
        return context;
    }

};

}

#endif
#endif
