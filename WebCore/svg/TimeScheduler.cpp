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

#include "config.h"
#if ENABLE(SVG)
#include "TimeScheduler.h"

#include "Document.h"
#include "SystemTime.h"
#include "SVGTimer.h"

namespace WebCore {

const double staticTimerInterval = 0.050; // 50 ms

TimeScheduler::TimeScheduler(Document* document)
    : m_creationTime(currentTime())
    , m_savedTime(0)
    , m_document(document)
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
#if ENABLE(SVG_ANIMATION)
    SVGTimer* svgTimer = new SVGTimer(this, ms * 0.001, true);
    svgTimer->addNotify(element, true);
    m_timerSet.add(svgTimer);
    m_intervalTimer->addNotify(element, false);
#endif
}

void TimeScheduler::connectIntervalTimer(SVGAnimationElement* element)
{
#if ENABLE(SVG_ANIMATION)
    m_intervalTimer->addNotify(element, true);
#endif
}

void TimeScheduler::disconnectIntervalTimer(SVGAnimationElement* element)
{
#if ENABLE(SVG_ANIMATION)
    m_intervalTimer->removeNotify(element);
#endif
}

void TimeScheduler::startAnimations()
{
#if ENABLE(SVG_ANIMATION)
    m_creationTime = currentTime();

    SVGTimerSet::iterator end = m_timerSet.end();
    for (SVGTimerSet::iterator it = m_timerSet.begin(); it != end; ++it) {
        SVGTimer* svgTimer = *it;
        if (svgTimer && !svgTimer->isActive())
            svgTimer->start();
    }
#endif
}

void TimeScheduler::toggleAnimations()
{
#if ENABLE(SVG_ANIMATION)
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
#endif
}

bool TimeScheduler::animationsPaused() const
{
    return !m_intervalTimer->isActive();
}

void TimeScheduler::timerFired(Timer<TimeScheduler>* baseTimer)
{
#if ENABLE(SVG_ANIMATION)
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
#endif
}

double TimeScheduler::elapsed() const
{
    return currentTime() - m_creationTime;
}

} // namespace

// vim:ts=4:noet
#endif // ENABLE(SVG)
