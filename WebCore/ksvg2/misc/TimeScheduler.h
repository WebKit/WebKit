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

#ifndef TimeScheduler_h
#define TimeScheduler_h
#if ENABLE(SVG)

#include <wtf/HashSet.h>

namespace WebCore {
    class Document;
    class SVGAnimationElement;
    class SVGTimer;

    template <typename T> class Timer;

    typedef HashSet<SVGTimer*> SVGTimerSet;

    class TimeScheduler {
    public:
        TimeScheduler(Document*);
        ~TimeScheduler();

        // Adds singleShot Timers
        void addTimer(SVGAnimationElement*, unsigned int ms);

        // (Dis-)Connects to interval timer with 'staticTimerInterval'
        void connectIntervalTimer(SVGAnimationElement*);
        void disconnectIntervalTimer(SVGAnimationElement*);

        void startAnimations();
        void toggleAnimations();
        bool animationsPaused() const;

        // time elapsed in seconds after creation of this object
        double elapsed() const;

    private:
        friend class SVGTimer;
        void timerFired(Timer<TimeScheduler>*);
        Document* document() const { return m_document; }

    private:
        double m_creationTime;
        double m_savedTime;
        
        SVGTimerSet m_timerSet;
        
        SVGTimer* m_intervalTimer;
        Document* m_document;
    };
}

#endif // ENABLE(SVG)
#endif

// vim:ts=4:noet
