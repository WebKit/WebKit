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

#include "config.h"
#ifdef SVG_SUPPORT
#include "SVGTimer.h"

#include <wtf/HashMap.h>
#include "SVGAnimateTransformElement.h"
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

double SVGTimer::calculateTimePercentage(double elapsedSeconds, double start, double end, double duration, double repetitions)
{
    double percentage = 0.0;

    double useElapsed = elapsedSeconds - (duration * repetitions);
    
    if (duration > 0.0 && end == 0.0)
        percentage = 1.0 - (((start + duration) - useElapsed) / duration);
    else if (duration > 0.0 && end != 0.0) {
        if (duration > end)
            percentage = 1.0 - (((start + end) - useElapsed) / end);
        else
            percentage = 1.0 - (((start + duration) - useElapsed) / duration);
    } else if (duration == 0.0 && end != 0.0)
        percentage = 1.0 - (((start + end) - useElapsed) / end);

    return percentage;
}

SVGTimer::TargetAnimationMap SVGTimer::animationsByElement(double elapsedSeconds)
{
    ExceptionCode ec = 0;
    TargetAnimationMap targetMap;
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
        
        SVGElement* target = const_cast<SVGElement *>(animation->targetElement());
        TargetAnimationMap::iterator i = targetMap.find(target);
        if (i != targetMap.end())
            i->second.append(animation);
        else {
            Vector<SVGAnimationElement*> list;
            list.append(animation);
            targetMap.set(target, list);
        }
    }
    return targetMap;
}

void SVGTimer::notifyAll()
{
    if (m_enabledNotifySet.isEmpty())
        return;

    double elapsedSeconds = m_scheduler->elapsed() * 1000.0; // Take time now.

    // First build a list of animation elements per target element
    // This is important to decide about the order & priority of 
    // the animations -> 'additive' support is handled this way.
    TargetAnimationMap targetMap = animationsByElement(elapsedSeconds);

    ExceptionCode ec = 0;

    TargetAnimationMap::iterator targetIterator = targetMap.begin();
    TargetAnimationMap::iterator tend = targetMap.end();
    for (; targetIterator != tend; ++targetIterator) {
        HashMap<String, Color> targetColor; // special <animateColor> case
        RefPtr<SVGTransformList> targetTransforms; // special <animateTransform> case    

        unsigned count = targetIterator->second.size();
        for (unsigned i = 0; i < count; ++i) {
            SVGAnimationElement* animation = targetIterator->second[i];

            double end = animation->getEndTime();
            double start = animation->getStartTime();
            double duration = animation->getSimpleDuration(ec);
            double repetitions = animation->repeations();

            // Validate animation timing settings:
            // #1 (duration > 0) -> fine
            // #2 (duration <= 0.0 && end > 0) -> fine
            
            if ((duration <= 0.0 && end <= 0.0) || (animation->isIndefinite(duration) && end <= 0.0))
                continue; // Ignore dur="0" or dur="-neg"
            
            float percentage = calculateTimePercentage(elapsedSeconds, start, end, duration, repetitions);
                
            if (percentage <= 1.0 || animation->connected())
                animation->handleTimerEvent(percentage);

            // Special cases for animate* objects depending on 'additive' attribute
            if (animation->hasTagName(SVGNames::animateTransformTag)) {
                SVGAnimateTransformElement* animTransform = static_cast<SVGAnimateTransformElement*>(animation);
                AffineTransform transformMatrix = animTransform->transformMatrix();
                RefPtr<SVGTransform> targetTransform = new SVGTransform();

                if (!targetTransforms) { // lazy creation, only if needed.
                    targetTransforms = new SVGTransformList();

                    if (animation->isAdditive()) {
                        targetTransform->setMatrix(animTransform->initialMatrix());
                        targetTransforms->appendItem(targetTransform.get(), ec);
                        targetTransform = new SVGTransform();
                    }
                }

                if (targetTransforms->numberOfItems() <= 1)
                    targetTransform->setMatrix(transformMatrix);
                else {
                    if (!animation->isAdditive())
                        targetTransforms->clear(ec);
                    targetTransform->setMatrix(transformMatrix);
                }

                targetTransforms->appendItem(targetTransform.get(), ec);
            } else if (animation->hasTagName(SVGNames::animateColorTag)) {
                SVGAnimateColorElement* animColor = static_cast<SVGAnimateColorElement*>(animation);
                String name = animColor->attributeName();

                if (animation->isAdditive()) {
                    if (targetColor.contains(name))
                        targetColor.set(name, animColor->addColorsAndClamp(targetColor.get(name), animColor->color()));
                    else
                        targetColor.set(name, animColor->addColorsAndClamp(animColor->initialColor(), animColor->color()));
                } else
                    targetColor.set(name, animColor->color());
            }
        }

        // Handle <animateTransform>.
        if (targetTransforms) {
            SVGElement* key = targetIterator->first;
            if (key && key->isStyled() && key->isStyledTransformable()) {
                SVGStyledTransformableElement* transform = static_cast<SVGStyledTransformableElement*>(key);
                transform->setTransform(targetTransforms.get());
                transform->updateLocalTransform(transform->transform());
            }
        }

        // Handle <animateColor>.
        HashMap<String, Color>::iterator colorIteratorEnd = targetColor.end();
        for (HashMap<String, Color>::iterator colorIterator = targetColor.begin(); colorIterator != colorIteratorEnd; ++colorIterator) {
            if (colorIterator->second.isValid())
                SVGAnimationElement::setTargetAttribute(targetIterator->first, colorIterator->first, colorIterator->second.name());
        }
    }

    // Make a second pass through the map to avoid multiple setChanged calls on the same element.
    for (targetIterator = targetMap.begin(); targetIterator != tend; ++targetIterator) {
        SVGElement* key = targetIterator->first;
        if (key && key->isStyled())
            static_cast<SVGStyledElement*>(key)->setChanged(true);
    }
}

void SVGTimer::addNotify(SVGAnimationElement* element, bool enabled)
{
    m_notifySet.add(element);
    if (enabled)
        m_enabledNotifySet.add(element);
    else
        m_enabledNotifySet.remove(element);
}

void SVGTimer::removeNotify(SVGAnimationElement *element)
{
    // FIXME: Why do we keep a pointer to the element forever (marked disabled)?
    // That can't be right!

    m_enabledNotifySet.remove(element);
    if (m_enabledNotifySet.isEmpty())
        stop();
}

} // namespace

// vim:ts=4:noet
#endif // SVG_SUPPORT
