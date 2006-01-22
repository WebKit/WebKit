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

#ifndef KSVG_TimeScheduler_H
#define KSVG_TimeScheduler_H
#if SVG_SUPPORT

#include <qtimer.h>
#include <qobject.h>
#include <qdatetime.h>
#include <q3valuelist.h>

#include "SVGAnimationElementImpl.h"

namespace KDOM {
    class DocumentImpl;
}

namespace KSVG
{
    typedef struct
    {
        bool enabled : 1;
        SVGAnimationElementImpl *animation;
    } SVGNotificationStruct;
    
    typedef Q3ValueList<SVGNotificationStruct> SVGNotifyList;

    class TimeScheduler;
    class SVGTimer
    {
    public:
        SVGTimer(TimeScheduler *scheduler, unsigned int ms, bool singleShot);
        ~SVGTimer();

        bool operator==(const QTimer *timer);
        const QTimer *qtimer() const;

        void start(QObject *receiver, const char *member);
        void stop();

        bool isActive() const;

        unsigned int ms() const;
        bool singleShot() const;

        void notifyAll();
        void addNotify(SVGAnimationElementImpl *element, bool enabled = false);
        void removeNotify(SVGAnimationElementImpl *element);

    private:
        double calculateTimePercentage(double elapsed, double start, double end, double duration, double repeations);

        unsigned int m_ms;
        bool m_invoked, m_singleShot;

        QTimer *m_timer;
        TimeScheduler *m_scheduler;
        SVGNotifyList m_notifyList;
    };

    typedef Q3ValueList<SVGTimer *> SVGTimerList;

    class TimeScheduler : public QObject
    {
    Q_OBJECT
    public:
        TimeScheduler(KDOM::DocumentImpl *doc);
        ~TimeScheduler();

        // Adds singleShot Timers
        void addTimer(SVGAnimationElementImpl *element, unsigned int ms);

        // (Dis-)Connects to interval timer with ms = 'staticTimerInterval'
        void connectIntervalTimer(SVGAnimationElementImpl *element);
        void disconnectIntervalTimer(SVGAnimationElementImpl *element);

        void startAnimations();
        void toggleAnimations();
        bool animationsPaused() const;

        // time elapsed in seconds after creation of this object
        float elapsed() const;

        static const unsigned int staticTimerInterval;

    private slots:
        void slotTimerNotify();

    private: // Helper
        friend class SVGTimer;
        KDOM::DocumentImpl *document() const { return m_document; }

    private:
        int m_savedTime;
        QTime m_creationTime;
        
        SVGTimerList m_timerList;
        
        SVGTimer *m_intervalTimer;
        KDOM::DocumentImpl *m_document;
    };
};

#endif // SVG_SUPPORT
#endif

// vim:ts=4:noet
