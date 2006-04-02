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
#if SVG_SUPPORT
#include "KSVGTimeScheduler.h"

#include "Document.h"
#include "SVGAnimateColorElement.h"
#include "SVGAnimateTransformElement.h"
#include "SVGAnimatedTransformList.h"
#include "SVGDOMImplementation.h"
#include "SVGMatrix.h"
#include "SVGNames.h"
#include "SVGStyledElement.h"
#include "SVGStyledTransformableElement.h"
#include "SystemTime.h"
#include "Timer.h"

namespace WebCore {

const double staticTimerInterval = 0.050; // 50 ms

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

    TimeScheduler* m_scheduler;
    double m_interval;
    bool m_singleShot;

    SVGNotifySet m_notifySet;
    SVGNotifySet m_enabledNotifySet;
};

SVGTimer::SVGTimer(TimeScheduler* scheduler, double interval, bool singleShot)
    : Timer<TimeScheduler>(scheduler, &TimeScheduler::timerFired)
    , m_scheduler(scheduler), m_interval(interval), m_singleShot(singleShot)
{
}

void SVGTimer::start()
{
    if (m_singleShot)
        startOneShot(m_interval);
    else
        startRepeating(m_interval);
}

double SVGTimer::calculateTimePercentage(double elapsed, double start, double end, double duration, double repetitions)
{
    double percentage = 0.0;

    double useElapsed = elapsed - (duration * repetitions);
    
    if (duration > 0.0 && end == 0.0)
        percentage = 1.0 - (((start + duration) - useElapsed) / duration);
    else if (duration > 0.0 && end != 0.0) {
        if (duration > end)
            percentage = 1.0 - (((start + end) - useElapsed) / end);
        else
            percentage = 1.0 - (((start + duration) - useElapsed) / duration);
    } else if(duration == 0.0 && end != 0.0)
        percentage = 1.0 - (((start + end) - useElapsed) / end);

    return percentage;
}

void SVGTimer::notifyAll()
{
    if (m_enabledNotifySet.isEmpty())
        return;

    double elapsed = m_scheduler->elapsed() * 1000.0; // Take time now.

    // First build a list of animation elements per target element
    // This is important to decide about the order & priority of 
    // the animations -> 'additive' support is handled this way.
    typedef HashMap<SVGElement*, Vector<SVGAnimationElement*> > TargetAnimationMap;
    TargetAnimationMap targetMap;
    
    SVGNotifySet::const_iterator end = m_notifySet.end();
    for (SVGNotifySet::const_iterator it = m_notifySet.begin(); it != end; ++it) {
        SVGAnimationElement* animation = *it;

        // If we're dealing with a disabled element with fill="freeze",
        // we have to take it into account for further calculations.
        if (!m_enabledNotifySet.contains(animation)) {
            if (!animation->isFrozen())
                continue;
            if (elapsed <= (animation->getStartTime() + animation->getSimpleDuration()))
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
            double duration = animation->getSimpleDuration();
            double repetitions = animation->repeations();

            // Validate animation timing settings:
            // #1 (duration > 0) -> fine
            // #2 (duration <= 0.0 && end > 0) -> fine
            
            if((duration <= 0.0 && end <= 0.0) ||
               (animation->isIndefinite(duration) && end <= 0.0)) // Ignore dur="0" or dur="-neg"
                continue;
            
            float percentage = calculateTimePercentage(elapsed, start, end, duration, repetitions);
                
            if(percentage <= 1.0 || animation->connected())
                animation->handleTimerEvent(percentage);

// FIXME: Disable animateTransform until SVGList can be fixed.
#if 0
            // Special cases for animate* objects depending on 'additive' attribute
            if(animation->hasTagName(SVGNames::animateTransformTag))
            {
                SVGAnimateTransformElement *animTransform = static_cast<SVGAnimateTransformElement *>(animation);
                if(!animTransform)
                    continue;

                RefPtr<SVGMatrix> transformMatrix = animTransform->transformMatrix();
                if(!transformMatrix)
                    continue;

                RefPtr<SVGMatrix> initialMatrix = animTransform->initialMatrix();
                RefPtr<SVGTransform> data = new SVGTransform();

                if(!targetTransforms) // lazy creation, only if needed.
                {
                    targetTransforms = new SVGTransformList();

                    if(animation->isAdditive() && initialMatrix)
                    {
                        RefPtr<SVGMatrix> matrix = new SVGMatrix(initialMatrix->qmatrix());
                        
                        data->setMatrix(matrix.get());
                        targetTransforms->appendItem(data.get());

                        data = new SVGTransform();
                    }
                }

                if(targetTransforms->numberOfItems() <= 1)
                    data->setMatrix(transformMatrix.get());
                else
                {
                    if(!animation->isAdditive())
                        targetTransforms->clear();    
                    
                    data->setMatrix(transformMatrix.get());
                }

                targetTransforms->appendItem(data.get());
            }
            else
#endif
            if(animation->hasTagName(SVGNames::animateColorTag))
            {
                SVGAnimateColorElement *animColor = static_cast<SVGAnimateColorElement *>(animation);
                if(!animColor)
                    continue;

                DeprecatedString name = animColor->attributeName();
                Color color = animColor->color();

                if(!targetColor.contains(name))
                {
                    if(animation->isAdditive())
                    {
                        int r = animColor->initialColor().red() + color.red();
                        int g = animColor->initialColor().green() + color.green();
                        int b = animColor->initialColor().blue() + color.blue();
                    
                        targetColor.set(name, animColor->clampColor(r, g, b));
                    }
                    else
                        targetColor.set(name, color);
                }
                else
                {
                    if(!animation->isAdditive())
                        targetColor.set(name, color);
                    else
                    {
                        Color baseColor = targetColor.get(name);
                        int r = baseColor.red() + color.red();
                        int g = baseColor.green() + color.green();
                        int b = baseColor.blue() + color.blue();

                        targetColor.set(name, animColor->clampColor(r, g, b));
                    }
                }
            }
        }

        // Handle <animateTransform>.
        if (targetTransforms) {
            SVGElement* key = targetIterator->first;
            if (key && key->isStyled() && key->isStyledTransformable()) {
                SVGStyledTransformableElement *transform = static_cast<SVGStyledTransformableElement *>(key);
                transform->transform()->setAnimVal(targetTransforms.get());
                transform->updateLocalTransform(transform->transform()->animVal());
            }
        }

        // Handle <animateColor>.
        HashMap<String, Color>::iterator cend = targetColor.end();
        for(HashMap<String, Color>::iterator cit = targetColor.begin(); cit != cend; ++cit)
        {
            if(cit->second.isValid())
            {
                SVGAnimationElement::setTargetAttribute(targetIterator->first,
                                                            cit->first.impl(),
                                                            String(cit->second.name()).impl());
            }
        }
    }

    // Make a second pass through the map to avoid multiple setChanged calls on the same element.
    for (targetIterator = targetMap.begin(); targetIterator != tend; ++targetIterator) {
        SVGElement *key = targetIterator->first;
        if (key && key->isStyled())
            static_cast<SVGStyledElement *>(key)->setChanged(true);
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

TimeScheduler::TimeScheduler(Document *document)
    : m_creationTime(currentTime()), m_savedTime(0), m_document(document)
{
    // Don't start this timer yet.
    m_intervalTimer = new SVGTimer(this, staticTimerInterval, false);
}

TimeScheduler::~TimeScheduler()
{
    deleteAllValues(m_timerSet);
    delete m_intervalTimer;
}

void TimeScheduler::addTimer(SVGAnimationElement* element, unsigned ms)
{
    SVGTimer* svgTimer = new SVGTimer(this, ms * 0.001, true);
    svgTimer->addNotify(element, true);
    m_timerSet.add(svgTimer);
    m_intervalTimer->addNotify(element, false);
}

void TimeScheduler::connectIntervalTimer(SVGAnimationElement* element)
{
    m_intervalTimer->addNotify(element, true);
}

void TimeScheduler::disconnectIntervalTimer(SVGAnimationElement* element)
{
    m_intervalTimer->removeNotify(element);
}

void TimeScheduler::startAnimations()
{
    m_creationTime = currentTime();

    SVGTimerSet::iterator end = m_timerSet.end();
    for (SVGTimerSet::iterator it = m_timerSet.begin(); it != end; ++it) {
        SVGTimer* svgTimer = *it;
        if (svgTimer && !svgTimer->isActive())
            svgTimer->start();
    }
}

void TimeScheduler::toggleAnimations()
{
    if (m_intervalTimer->isActive()) {
        m_intervalTimer->stop();
        m_savedTime = currentTime();
    } else {
        if (m_savedTime != 0) {
            m_creationTime += currentTime() - m_savedTime;
            m_savedTime = 0;
        }
        m_intervalTimer->start();
    }
}

bool TimeScheduler::animationsPaused() const
{
    return !m_intervalTimer->isActive();
}

void TimeScheduler::timerFired(Timer<TimeScheduler>* baseTimer)
{
    // Get the pointer now, because notifyAll could make the document,
    // including this TimeScheduler, go away.
    RefPtr<Document> doc = m_document;

    SVGTimer* timer = SVGTimer::downcast(baseTimer);

    timer->notifyAll();

    // FIXME: Is it really safe to look at m_intervalTimer now?
    // Isn't it possible the TimeScheduler was deleted already?
    // If so, timer, m_timerSet, and m_intervalTimer have all
    // been deleted. May need to reference count the TimeScheduler
    // to work around this, and ref/deref it in this function.
    if (timer != m_intervalTimer) {
        ASSERT(!timer->isActive());
        ASSERT(m_timerSet.contains(timer));
        m_timerSet.remove(timer);
        delete timer;

        // The singleShot timers of ie. <animate> with begin="3s" are notified
        // by the previous call, and now all connections to the interval timer
        // are created and now we just need to fire that timer (Niko)
        if (!m_intervalTimer->isActive())
            m_intervalTimer->start();
    }

    // Update any 'dirty' shapes.
    doc->updateRendering();
}

double TimeScheduler::elapsed() const
{
    return currentTime() - m_creationTime;
}

} // namespace

// vim:ts=4:noet
#endif // SVG_SUPPORT
