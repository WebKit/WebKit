/*
    Copyright (C) Research In Motion Limited 2009. All rights reserved.

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

#ifndef SynchronizablePropertyController_h
#define SynchronizablePropertyController_h

#if ENABLE(SVG)
#include "StringHash.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class QualifiedName;
class SVGAnimatedPropertyBase;

class SynchronizableProperties {
public:
    SynchronizableProperties()
        : m_shouldSynchronize(false)
    {
    }

    void setNeedsSynchronization()
    {
        m_shouldSynchronize = true;
    }

    void addProperty(SVGAnimatedPropertyBase*);
    void synchronize();
    void startAnimation();
    void stopAnimation();

private:
    typedef HashSet<SVGAnimatedPropertyBase*> BaseSet;

    BaseSet m_bases;
    bool m_shouldSynchronize;
};

// Helper class used exclusively by SVGElement to keep track of all animatable properties within a SVGElement,
// and wheter they are supposed to be synchronized or not (depending wheter AnimatedPropertyTearOff's have been created)
class SynchronizablePropertyController : public Noncopyable {
public:
    void registerProperty(const QualifiedName&, SVGAnimatedPropertyBase*);
    void setPropertyNeedsSynchronization(const QualifiedName&);

    void synchronizeProperty(const String&);
    void synchronizeAllProperties();

    void startAnimation(const String&);
    void stopAnimation(const String&);

private:
    friend class SVGElement;
    SynchronizablePropertyController();

private:
    typedef HashMap<String, SynchronizableProperties> PropertyMap;

    PropertyMap m_map;
};

};

#endif // ENABLE(SVG)
#endif // SynchronizablePropertyController_h
