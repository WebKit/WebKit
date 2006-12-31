/*
 * Copyright (C) 2006 Eric Seidel <eric@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PathTraversalState_H
#define PathTraversalState_H

#include "FloatPoint.h"
#include <wtf/Vector.h>

namespace WebCore {
    
    class Path;
    
    class PathTraversalState {
    public:
        enum PathTraversalAction {
            TraversalTotalLength,
            TraversalPointAtLength,
            TraversalSegmentAtLength, // not yet implemented
            TraversalPointAndAnglesForOffsets // not yet implemented
        };
        
        PathTraversalState(PathTraversalAction);
        
        float closeSubpath();
        float moveTo(const FloatPoint&);
        float lineTo(const FloatPoint&);
        float quadraticBezierTo(const FloatPoint& newControl, const FloatPoint& newEnd);
        float cubicBezierTo(const FloatPoint& newControl1, const FloatPoint& newControl2, const FloatPoint& newEnd);
        
    public:
        PathTraversalAction m_action;
        bool m_success;
        
        FloatPoint m_current;
        FloatPoint m_start;
        FloatPoint m_control1;
        FloatPoint m_control2;
        
        float m_totalLength;
        unsigned m_segmentIndex; // for segment finding (not implemented)
        float m_desiredLength;
        
        // FIXME: for (non-implemented) text-on-path layout
        Vector<float> m_offsets;
        Vector< std::pair<FloatPoint, float> > m_pointAndAngles;
    };    
}

#endif
