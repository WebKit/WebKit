/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

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
#include <kcanvas/KCanvas.h>

#include "DocumentImpl.h"
#include "KSVGTimeScheduler.h"
#include "SVGAnimateColorElementImpl.h"
#include "SVGAnimateTransformElementImpl.h"
#include "SVGAnimatedTransformListImpl.h"
#include "SVGDOMImplementationImpl.h"
#include "SVGMatrixImpl.h"
#include "SVGNames.h"
#include "SVGStyledElementImpl.h"
#include "SVGStyledTransformableElementImpl.h"

namespace KSVG {

SVGTimer::SVGTimer(TimeScheduler *scheduler, unsigned int ms, bool singleShot)
{
    m_ms = ms;
    m_scheduler = scheduler;
    m_singleShot = singleShot;
    
    m_timer = new QTimer();
}

SVGTimer::~SVGTimer()
{
    delete m_timer;
}

bool SVGTimer::operator==(const QTimer *timer)
{
    return (m_timer == timer);
}

const QTimer *SVGTimer::qtimer() const
{
    return m_timer;
}

void SVGTimer::start(QObject *receiver, const char *member)
{
    QObject::connect(m_timer, SIGNAL(timeout()), receiver, member);
    m_timer->start(m_ms, m_singleShot);
}

void SVGTimer::stop()
{
    m_timer->stop();
}

bool SVGTimer::isActive() const
{
    return m_timer->isActive();
}

unsigned int SVGTimer::ms() const
{
    return m_ms;
}

bool SVGTimer::singleShot() const
{
    return m_singleShot;
}
        
double SVGTimer::calculateTimePercentage(double elapsed, double start, double end, double duration, double repeations)
{
    double percentage = 0.0;

    // Take into account repeations...
    double useElapsed = elapsed - (duration * repeations);
    
    if(duration > 0.0 && end == 0.0)
        percentage = 1.0 - (((start + duration) - useElapsed) / duration);
    else if(duration > 0.0 && end != 0.0)
    {
        if(duration > end)
            percentage = 1.0 - (((start + end) - useElapsed) / end);
        else
            percentage = 1.0 - (((start + duration) - useElapsed) / duration);
    }
    else if(duration == 0.0 && end != 0.0)
        percentage = 1.0 - (((start + end) - useElapsed) / end);

    // kdDebug() << "RETURNING PERCENTAGE: " << percentage << " (elapsed: " << elapsed << " -> " << useElapsed << " start: " << start << " end: " << end << " dur: " << duration << " rep: " << repeations << ")" << endl;
    return percentage;
}

void SVGTimer::notifyAll()
{
    bool stillRunning = false;
    for(unsigned int i = m_notifyList.count(); i > 0; i--)
    {
        SVGNotificationStruct notifyStruct = m_notifyList[i - 1];

        if(notifyStruct.enabled)
        {
            stillRunning = true;
            continue;
        }
    }

    if(!stillRunning)
        return;

    double elapsed = m_scheduler->elapsed() * 1000.0; // Take time now...

    // First build a list of animation elements per target element
    // This is important to decide about the order & priority of 
    // the animations -> 'additive' support is handled this way.
    typedef HashMap<SVGElementImpl*, Q3PtrList<SVGAnimationElementImpl>, PointerHash<SVGElementImpl*> > TargetAnimationMap;
    TargetAnimationMap targetMap;
    
    for(unsigned int i = m_notifyList.count(); i > 0; i--)
    {
        SVGNotificationStruct notifyStruct = m_notifyList[i - 1];

        if(!notifyStruct.animation)
            continue;

        SVGAnimationElementImpl *animation = notifyStruct.animation;

        // If we're dealing with a disabled element with fill="freeze",
        // we have to take it into account for further calculations... 
        if(!notifyStruct.enabled)
        {
            if(animation->isFrozen())
            {
                if(elapsed <= (animation->getStartTime() + animation->getSimpleDuration()))
                    continue;
            }
            else
                continue;
        }

        SVGElementImpl *target = const_cast<SVGElementImpl *>(animation->targetElement());
        TargetAnimationMap::iterator i = targetMap.find(target);
        if (i != targetMap.end())
            i->second.prepend(animation);
        else {
            Q3PtrList<SVGAnimationElementImpl> list;
            list.append(animation);
            targetMap.set(target, list);
        }
    }

    TargetAnimationMap::iterator tit = targetMap.begin();
    TargetAnimationMap::iterator tend = targetMap.end();

    for(; tit != tend; ++tit)
    {
        Q3PtrList<SVGAnimationElementImpl>::Iterator it = tit->second.begin();
        Q3PtrList<SVGAnimationElementImpl>::Iterator end = tit->second.end();

        HashMap<DOMString, QColor> targetColor; // special <animateColor> case
        RefPtr<SVGTransformListImpl> targetTransforms; // special <animateTransform> case    

        for(; it != end; ++it)
        {
            SVGAnimationElementImpl *animation = (*it);

            double end = animation->getEndTime();
            double start = animation->getStartTime();
            double duration = animation->getSimpleDuration();
            double repeations = animation->repeations();

            // Validate animation timing settings:
            // #1 (duration > 0) -> fine
            // #2 (duration <= 0.0 && end > 0) -> fine
            
            if((duration <= 0.0 && end <= 0.0) ||
               (animation->isIndefinite(duration) && end <= 0.0)) // Ignore dur="0" or dur="-neg"...
                continue;
            
            float percentage = calculateTimePercentage(elapsed, start, end, duration, repeations);
                
            if(percentage <= 1.0 || animation->connected())
                animation->handleTimerEvent(percentage);

            // Special cases for animate* objects depending on 'additive' attribute
            if(animation->hasTagName(SVGNames::animateTransformTag))
            {
                SVGAnimateTransformElementImpl *animTransform = static_cast<SVGAnimateTransformElementImpl *>(animation);
                if(!animTransform)
                    continue;

                RefPtr<SVGMatrixImpl> transformMatrix = animTransform->transformMatrix();
                if(!transformMatrix)
                    continue;

                RefPtr<SVGMatrixImpl> initialMatrix = animTransform->initialMatrix();
                RefPtr<SVGTransformImpl> data = new SVGTransformImpl();

                if(!targetTransforms) // lazy creation, only if needed.
                {
                    targetTransforms = new SVGTransformListImpl();

                    if(animation->isAdditive() && initialMatrix)
                    {
                        RefPtr<SVGMatrixImpl> matrix = new SVGMatrixImpl(initialMatrix->qmatrix());
                        
                        data->setMatrix(matrix.get());
                        targetTransforms->appendItem(data.get());

                        data = new SVGTransformImpl();
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
            else if(animation->hasTagName(SVGNames::animateColorTag))
            {
                SVGAnimateColorElementImpl *animColor = static_cast<SVGAnimateColorElementImpl *>(animation);
                if(!animColor)
                    continue;

                QString name = animColor->attributeName();
                QColor color = animColor->color();

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
                        QColor baseColor = targetColor.get(name);
                        int r = baseColor.red() + color.red();
                        int g = baseColor.green() + color.green();
                        int b = baseColor.blue() + color.blue();

                        targetColor.set(name, animColor->clampColor(r, g, b));
                    }
                }
            }
        }

        // Handle <animateTransform>...
        if(targetTransforms)
        {
            SVGElementImpl *key = tit->first;
            if(key && key->isStyled() && key->isStyledTransformable())
            {
                SVGStyledTransformableElementImpl *transform = static_cast<SVGStyledTransformableElementImpl *>(key);
                transform->transform()->setAnimVal(targetTransforms.get());
                transform->updateLocalTransform(transform->transform()->animVal());
            }
        }

        // Handle <animateColor>...
        HashMap<DOMString, QColor>::iterator cend = targetColor.end();
        for(HashMap<DOMString, QColor>::iterator cit = targetColor.begin(); cit != cend; ++cit)
        {
            if(cit->second.isValid())
            {
                SVGAnimationElementImpl::setTargetAttribute(tit->first,
                                                            cit->first.impl(),
                                                            KDOM::DOMString(cit->second.name()).impl());
            }
        }
    }

    // Optimized update logic (to avoid 4 updates, on the same element)
    for(tit = targetMap.begin(); tit != tend; ++tit)
    {
        SVGElementImpl *key = tit->first;
        if(key && key->isStyled())
            static_cast<SVGStyledElementImpl *>(key)->setChanged(true);
    }
}

void SVGTimer::addNotify(SVGAnimationElementImpl *element, bool enabled)
{
    for(unsigned int i = m_notifyList.count(); i > 0; i--)
    {
        SVGNotificationStruct &notifyStruct = m_notifyList[i - 1];
        if(notifyStruct.animation == element)
        {
            notifyStruct.enabled = enabled;
            return;
        }
    }

    SVGNotificationStruct notifyStruct;
    notifyStruct.animation = element;
    notifyStruct.enabled = enabled;
    
    m_notifyList.append(notifyStruct);
}

void SVGTimer::removeNotify(SVGAnimationElementImpl *element)
{
    for(unsigned int i = m_notifyList.count(); i > 0; i--)
    {
        SVGNotificationStruct &notifyStruct = m_notifyList[i - 1];
        if(notifyStruct.animation == element)
        {
            notifyStruct.enabled = false;
            break;
        }
    }

    bool active = false;
    for(unsigned int i = m_notifyList.count(); i > 0; i--)
    {
        SVGNotificationStruct notifyStruct = m_notifyList[i - 1];
        if(notifyStruct.enabled)
        {
            active = true;
            break;
        }
    }
    
    if(!active)
        stop();
}

const unsigned int TimeScheduler::staticTimerInterval = 50; // milliseconds

TimeScheduler::TimeScheduler(KDOM::DocumentImpl *document) : QObject(), m_document(document)
{
    // Create static interval timers but don't start it yet!
    m_intervalTimer = new SVGTimer(this, staticTimerInterval, false);

    m_savedTime = 0;
    m_creationTime = QTime::currentTime();
}

TimeScheduler::~TimeScheduler()
{
    // Usually singleShot timers cleanup themselves, after usage
    SVGTimerList::const_iterator it = m_timerList.begin();
    SVGTimerList::const_iterator end = m_timerList.end();

    for(; it != end; ++it)
        delete (*it);
    
    delete m_intervalTimer;
}

void TimeScheduler::addTimer(SVGAnimationElementImpl *element, unsigned int ms)
{
    SVGTimer *svgTimer = new SVGTimer(this, ms, true);
    svgTimer->addNotify(element, true);
    m_timerList.append(svgTimer);

    m_intervalTimer->addNotify(element, false);
}

void TimeScheduler::connectIntervalTimer(SVGAnimationElementImpl *element)
{
    m_intervalTimer->addNotify(element, true);
}

void TimeScheduler::disconnectIntervalTimer(SVGAnimationElementImpl *element)
{
    m_intervalTimer->removeNotify(element);
}

void TimeScheduler::startAnimations()
{
    m_creationTime.start();

    SVGTimerList::iterator it = m_timerList.begin();
    SVGTimerList::iterator end = m_timerList.end();

    for(; it != end; ++it)
    {
        SVGTimer *svgTimer = *it;
        if(svgTimer && !svgTimer->isActive())
            svgTimer->start(this, SLOT(slotTimerNotify()));
    }
}

void TimeScheduler::toggleAnimations()
{
    if(m_intervalTimer->isActive())
    {
        m_intervalTimer->stop();
        m_savedTime = m_creationTime.elapsed();
    }
    else
    {
        if(m_savedTime != 0)
            m_creationTime = m_creationTime.addMSecs(m_creationTime.elapsed() - m_savedTime);

        m_intervalTimer->start(this, SLOT(slotTimerNotify()));
    }
}

bool TimeScheduler::animationsPaused() const
{
    return !m_intervalTimer->isActive();
}

void TimeScheduler::slotTimerNotify()
{
    QTimer *senderTimer = const_cast<QTimer *>(static_cast<const QTimer *>(sender()));
    SVGTimer *svgTimer = 0;

    SVGTimerList::iterator it = m_timerList.begin();
    SVGTimerList::iterator end = m_timerList.end();
    for(; it != end; ++it)
    {
        SVGTimer *cur = *it;
        if(*cur == senderTimer)
        {
            svgTimer = cur;
            break;
        }
    }

    if(!svgTimer)
    {
        svgTimer = (*m_intervalTimer == senderTimer) ? m_intervalTimer : 0;
    
        if(!svgTimer)
            return;
    }

    svgTimer->notifyAll();

    // Update any 'dirty' shapes...
    m_document->updateRendering();
        
    if(svgTimer->singleShot())
    {
        m_timerList.remove(svgTimer);
        delete svgTimer;
    }

    // The singleShot timers of ie. <animate> with begin="3s" are notified
    // by the previous call, and now all connections to the interval timer
    // are created and now we just need to fire that timer (Niko)
    if(svgTimer != m_intervalTimer && !m_intervalTimer->isActive())
        m_intervalTimer->start(this, SLOT(slotTimerNotify()));
}

float TimeScheduler::elapsed() const
{
    return float(m_creationTime.elapsed()) / 1000.0;
}

} // namespace;

// vim:ts=4:noet
