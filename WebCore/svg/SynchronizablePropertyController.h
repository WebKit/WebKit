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

struct SynchronizableProperty {
    SynchronizableProperty()
        : base(0)
        , shouldSynchronize(false)
    {
    }

    explicit SynchronizableProperty(SVGAnimatedPropertyBase* _base)
        : base(_base)
        , shouldSynchronize(false)
    {
    }

    explicit SynchronizableProperty(WTF::HashTableDeletedValueType)
        : base(reinterpret_cast<SVGAnimatedPropertyBase*>(-1))
        , shouldSynchronize(false)
    {
    }

    bool isHashTableDeletedValue() const
    {
        return base == reinterpret_cast<SVGAnimatedPropertyBase*>(-1);
    }

    bool operator==(const SynchronizableProperty& other) const
    {
        return base == other.base && shouldSynchronize == other.shouldSynchronize;
    }

    SVGAnimatedPropertyBase* base;
    bool shouldSynchronize;
};

struct SynchronizablePropertyHash {
    static unsigned hash(const SynchronizableProperty& key)
    {
        return StringImpl::computeHash(reinterpret_cast<const UChar*>(&key), sizeof(SynchronizableProperty) / sizeof(UChar));
    }

    static bool equal(const SynchronizableProperty& a, const SynchronizableProperty& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct SynchronizablePropertyHashTraits : WTF::GenericHashTraits<SynchronizableProperty> {
    static const bool emptyValueIsZero = true;

    static void constructDeletedValue(SynchronizableProperty& slot)
    {
        new (&slot) SynchronizableProperty(WTF::HashTableDeletedValue);
    }

    static bool isDeletedValue(const SynchronizableProperty& value)
    {
        return value.isHashTableDeletedValue();
    }
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
    typedef HashSet<SynchronizableProperty, SynchronizablePropertyHash, SynchronizablePropertyHashTraits> Properties;
    typedef HashMap<String, Properties> PropertyMap;

    PropertyMap m_map;
};

};

#endif // ENABLE(SVG)
#endif // SynchronizablePropertyController_h
