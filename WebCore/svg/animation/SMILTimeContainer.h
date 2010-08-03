/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SMILTimeContainer_h
#define SMILTimeContainer_h

#if ENABLE(SVG)

#include "PlatformString.h"
#include "SMILTime.h"
#include "StringHash.h"
#include "Timer.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {
    
    class SVGElement;
    class SVGSMILElement;
    class SVGSVGElement;

    class SMILTimeContainer : public RefCounted<SMILTimeContainer>  {
    public:
        static PassRefPtr<SMILTimeContainer> create(SVGSVGElement* owner) { return adoptRef(new SMILTimeContainer(owner)); } 
    
        void schedule(SVGSMILElement*);
        void unschedule(SVGSMILElement*);
        
        SMILTime elapsed() const;

        bool isActive() const;
        bool isPaused() const;
        
        void begin();
        void pause();
        void resume();
        
        void setDocumentOrderIndexesDirty() { m_documentOrderIndexesDirty = true; }

        // Move to a specific time. Only used for DRT testing purposes.
        void sampleAnimationAtTime(const String& elementId, double seconds);

    private:
        SMILTimeContainer(SVGSVGElement* owner);
        
        void timerFired(Timer<SMILTimeContainer>*);
        void startTimer(SMILTime fireTime, SMILTime minimumDelay = 0);
        void updateAnimations(SMILTime elapsed);
        
        void updateDocumentOrderIndexes();
        void sortByPriority(Vector<SVGSMILElement*>& smilElements, SMILTime elapsed);
        
        typedef pair<SVGElement*, String> ElementAttributePair;
        String baseValueFor(ElementAttributePair);
        
        double m_beginTime;
        double m_pauseTime;
        double m_accumulatedPauseTime;
        double m_nextManualSampleTime;
        String m_nextSamplingTarget;

        bool m_documentOrderIndexesDirty;
        
        Timer<SMILTimeContainer> m_timer;

        typedef HashSet<SVGSMILElement*> TimingElementSet;
        TimingElementSet m_scheduledAnimations;
        
        typedef HashMap<ElementAttributePair, String> BaseValueMap;
        BaseValueMap m_savedBaseValues;

        SVGSVGElement* m_ownerSVGElement;
    };
}

#endif
#endif
