/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.

    This file is part of the KDE project

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

#if ENABLE(SVG)

#include "TimeScheduler.h"
#include "Timer.h"
#include <wtf/HashSet.h>
#include <wtf/HashMap.h>

namespace WebCore {

class SVGElement;
class SVGAnimationElement;

typedef HashSet<SVGAnimationElement*> SVGNotifySet;

class SVGTimer : private Timer<TimeScheduler>
{
public:
    SVGTimer(TimeScheduler*, double interval, bool singleShot);

    void start();
    using Timer<TimeScheduler>::stop;
    using Timer<TimeScheduler>::isActive;

    void notifyAll();
    void addNotify(SVGAnimationElement*, bool enabled = false);
    void removeNotify(SVGAnimationElement*);

    static SVGTimer* downcast(Timer<TimeScheduler>* t) { return static_cast<SVGTimer*>(t); }

private:
    typedef HashMap<SVGElement*, Vector<SVGAnimationElement*> > TargetAnimationMap;
    TargetAnimationMap animationsByElement(double elapsedTime);
    void applyAnimations(double elapsedSeconds, const SVGTimer::TargetAnimationMap& targetMap);

    TimeScheduler* m_scheduler;
    double m_interval;
    bool m_singleShot;

    SVGNotifySet m_notifySet;
    SVGNotifySet m_enabledNotifySet;
};

} // namespace

#endif // ENABLE(SVG)

// vim:ts=4:noet
