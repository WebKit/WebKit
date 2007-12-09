/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
    Copyright (C) 2006 Apple Computer, Inc.
    Copyright (C) 2007 Eric Seidel <eric@webkit.org>

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

#include "config.h"
#if ENABLE(SVG)
#include "SVGTimer.h"

#include <wtf/HashMap.h>
#include "SVGAnimateTransformElement.h"
#include "SVGAnimateMotionElement.h"
#include "SVGTransformList.h"
#include "SVGAnimateColorElement.h"
#include "SVGStyledTransformableElement.h"

namespace WebCore {

SVGTimer::SVGTimer(TimeScheduler* scheduler, double interval, bool singleShot)
    : Timer<TimeScheduler>(scheduler, &TimeScheduler::timerFired)
    , m_scheduler(scheduler)
    , m_interval(interval)
    , m_singleShot(singleShot)
{
}

void SVGTimer::start()
{
    if (m_singleShot)
        startOneShot(m_interval);
    else
        startRepeating(m_interval);
}

SVGTimer::TargetAnimationMap SVGTimer::animationsByElement(double elapsedSeconds)
{
    // Build a list of all animations which apply to each element
    // FIXME: This list should be sorted by animation priority
    TargetAnimationMap targetMap;
#if ENABLE(SVG_ANIMATION)
    ExceptionCode ec = 0;
    SVGNotifySet::const_iterator end = m_notifySet.end();
    for (SVGNotifySet::const_iterator it = m_notifySet.begin(); it != end; ++it) {
        SVGAnimationElement* animation = *it;
        
        // If we're dealing with a disabled element with fill="freeze",
        // we have to take it into account for further calculations.
        if (!m_enabledNotifySet.contains(animation)) {
            if (!animation->isFrozen())
                continue;
            if (elapsedSeconds <= (animation->getStartTime() + animation->getSimpleDuration(ec)))
                continue;
        }
        
        SVGElement* target = const_cast<SVGElement*>(animation->targetElement());
        TargetAnimationMap::iterator i = targetMap.find(target);
        if (i != targetMap.end())
            i->second.append(animation);
        else {
            Vector<SVGAnimationElement*> list;
            list.append(animation);
            targetMap.set(target, list);
        }
    }
#endif
    return targetMap;
}

// FIXME: This funtion will eventually become part of the AnimationCompositor
void SVGTimer::applyAnimations(double elapsedSeconds, const SVGTimer::TargetAnimationMap& targetMap)
{    
#if ENABLE(SVG_ANIMATION)
    TargetAnimationMap::const_iterator targetIterator = targetMap.begin();
    TargetAnimationMap::const_iterator tend = targetMap.end();
    for (; targetIterator != tend; ++targetIterator) {
        // FIXME: This is still not 100% correct.  Correct would be:
        // 1. Walk backwards through the priority list until a replace (!isAdditive()) is found
        // -- This optimization is not possible without careful consideration for dependent values (such as cx and fx in SVGRadialGradient)
        // 2. Set the initial value (or last replace) as the new animVal
        // 3. Call each enabled animation in turn, to have it apply its changes
        // 4. After building a new animVal, set it on the element.
        
        // Currenly we use the actual animVal on the element as "temporary storage"
        // and abstract the getting/setting of the attributes into the SVGAnimate* classes
        
        unsigned count = targetIterator->second.size();
        for (unsigned i = 0; i < count; ++i) {
            SVGAnimationElement* animation = targetIterator->second[i];
            
            if (!animation->isValidAnimation())
                continue;
            
            if (!animation->updateAnimationBaseValueFromElement())
                continue;
            
            if (!animation->updateAnimatedValueForElapsedSeconds(elapsedSeconds))
                continue;
            
            animation->applyAnimatedValueToElement();
        }
    }
    
    // Make a second pass through the map to avoid multiple setChanged calls on the same element.
    for (targetIterator = targetMap.begin(); targetIterator != tend; ++targetIterator) {
        SVGElement* key = targetIterator->first;
        if (key && key->isStyled())
            static_cast<SVGStyledElement*>(key)->setChanged();
    }
#endif
}

void SVGTimer::notifyAll()
{
#if ENABLE(SVG_ANIMATION)
    if (m_enabledNotifySet.isEmpty())
        return;

    // First build a list of animation elements per target element
    double elapsedSeconds = m_scheduler->elapsed() * 1000.0; // Take time now.
    TargetAnimationMap targetMap = animationsByElement(elapsedSeconds);
    
    // Then composite those animations down to final values and apply
    applyAnimations(elapsedSeconds, targetMap);
#endif
}

void SVGTimer::addNotify(SVGAnimationElement* element, bool enabled)
{
#if ENABLE(SVG_ANIMATION)
    m_notifySet.add(element);
    if (enabled)
        m_enabledNotifySet.add(element);
    else
        m_enabledNotifySet.remove(element);
#endif
}

void SVGTimer::removeNotify(SVGAnimationElement *element)
{
#if ENABLE(SVG_ANIMATION)
    // FIXME: Why do we keep a pointer to the element forever (marked disabled)?
    // That can't be right!

    m_enabledNotifySet.remove(element);
    if (m_enabledNotifySet.isEmpty())
        stop();
#endif
}

} // namespace

// vim:ts=4:noet
#endif // ENABLE(SVG)
