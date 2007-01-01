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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifdef SVG_SUPPORT

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
    double calculateTimePercentage(double elapsed, double start, double end, double duration, double repetitions);
    
    typedef HashMap<SVGElement*, Vector<SVGAnimationElement*> > TargetAnimationMap;
    TargetAnimationMap animationsByElement(double elapsedTime);

    TimeScheduler* m_scheduler;
    double m_interval;
    bool m_singleShot;

    SVGNotifySet m_notifySet;
    SVGNotifySet m_enabledNotifySet;
};

} // namespace

#endif // SVG_SUPPORT

// vim:ts=4:noet
