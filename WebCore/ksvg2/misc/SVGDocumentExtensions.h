/*
    Copyright (C) 2006 Apple Computer, Inc.

    This file is part of the WebKit project

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef SVGDocumentExtensions_H
#define SVGDocumentExtensions_H

#ifdef SVG_SUPPORT

#include <wtf/Forward.h>
#include <wtf/HashSet.h>
#include <wtf/HashMap.h>

#include "FloatRect.h"
#include "StringHash.h"
#include "StringImpl.h"
#include "AtomicString.h"

namespace WebCore {

class AtomicString;
class Document;
class EventListener;
class Node;
class SVGElement;
class String;
class TimeScheduler;
class SVGPathSeg;
class SVGStyledElement;
class SVGSVGElement;

class SVGDocumentExtensions {
public:
    SVGDocumentExtensions(Document*);
    ~SVGDocumentExtensions();
    
    PassRefPtr<EventListener> createSVGEventListener(const String& functionName, const String& code, Node*);
    
    void addTimeContainer(SVGSVGElement*);
    void removeTimeContainer(SVGSVGElement*);
    
    void startAnimations();
    void pauseAnimations();
    void unpauseAnimations();

private:
    Document* m_doc; // weak reference
    HashSet<SVGSVGElement*> m_timeContainers; // For SVG 1.2 support this will need to be made more general.

    SVGDocumentExtensions(const SVGDocumentExtensions&);
    SVGDocumentExtensions& operator=(const SVGDocumentExtensions&);

    template<typename ValueType>
    HashMap<const SVGElement*, HashMap<StringImpl*, ValueType>*> baseValueMap() const
    {
        static HashMap<const SVGElement*, HashMap<StringImpl*, ValueType>*> s_valueType;
        return s_valueType;
    }

    HashMap<const SVGPathSeg*, const SVGStyledElement*> pathSegContextMap() const
    {
        static HashMap<const SVGPathSeg*, const SVGStyledElement*> s_valueType;
        return s_valueType;
    }

public:
    // Used by the ANIMATED_PROPERTY_* macros
    template<typename ValueType>
    ValueType baseValue(const SVGElement* element, const AtomicString& propertyName) const
    {
        HashMap<StringImpl*, ValueType>* propertyMap = baseValueMap<ValueType>().get(element);
        if (propertyMap)
            return propertyMap->get(propertyName.impl());

        return 0;
    }

    template<typename ValueType>
    void setBaseValue(const SVGElement* element, const AtomicString& propertyName, ValueType newValue)
    {
        HashMap<StringImpl*, ValueType>* propertyMap = baseValueMap<ValueType>().get(element);
        if (!propertyMap) {
            propertyMap = new HashMap<StringImpl*, ValueType>();
            baseValueMap<ValueType>().set(element, propertyMap);
        }

        propertyMap->set(propertyName.impl(), newValue);
    }

    template<typename ValueType>
    void removeBaseValue(const SVGElement* element, const AtomicString& propertyName)
    {
        HashMap<StringImpl*, ValueType>* propertyMap = baseValueMap<ValueType>().get(element);
        if (!propertyMap)
            return;

        propertyMap->remove(propertyName.impl());
    }

    template<typename ValueType>
    bool hasBaseValue(const SVGElement* element, const AtomicString& propertyName) const
    {
        HashMap<StringImpl*, ValueType>* propertyMap = baseValueMap<ValueType>().get(element);
        if (propertyMap)
            return propertyMap->contains(propertyName.impl());

        return false;
    }

    // Used by SVGPathSeg* JS wrappers
    const SVGStyledElement* pathSegContext(const SVGPathSeg* obj) const
    {
        return pathSegContextMap().get(obj);
    }

    void setPathSegContext(const SVGPathSeg* obj, const SVGStyledElement* context)
    {
        pathSegContextMap().set(obj, context);
    }

    void removePathSegContext(const SVGPathSeg* obj)
    {
        pathSegContextMap().remove(obj);
    }

    bool hasPathSegContext(const SVGPathSeg* obj)
    {
        return pathSegContextMap().contains(obj);
    }
};

// Special handling for WebCore::String
template<>
inline String SVGDocumentExtensions::baseValue<String>(const SVGElement* element, const AtomicString& propertyName) const
{
    HashMap<StringImpl*, String>* propertyMap = baseValueMap<String>().get(element);
    if (propertyMap)
        return propertyMap->get(propertyName.impl());

    return String();
}

// Special handling for WebCore::FloatRect
template<>
inline FloatRect SVGDocumentExtensions::baseValue<FloatRect>(const SVGElement* element, const AtomicString& propertyName) const
{
    HashMap<StringImpl*, FloatRect>* propertyMap = baseValueMap<FloatRect>().get(element);
    if (propertyMap)
        return propertyMap->get(propertyName.impl());

    return FloatRect();
}

// Special handling for booleans
template<>
inline bool SVGDocumentExtensions::baseValue<bool>(const SVGElement* element, const AtomicString& propertyName) const
{
    HashMap<StringImpl*, bool>* propertyMap = baseValueMap<bool>().get(element);
    if (propertyMap)
        return propertyMap->get(propertyName.impl());

    return false;
}

// Special handling for doubles
template<>
inline double SVGDocumentExtensions::baseValue<double>(const SVGElement* element, const AtomicString& propertyName) const
{
    HashMap<StringImpl*, double>* propertyMap = baseValueMap<double>().get(element);
    if (propertyMap)
        return propertyMap->get(propertyName.impl());

    return 0.0;
}

}

#endif // SVG_SUPPORT

#endif
