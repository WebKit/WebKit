/*
 * Copyright (C) 2006, 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2015 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PathTraversalState_h
#define PathTraversalState_h

#include "FloatPoint.h"
#include "Path.h"

namespace WebCore {

class PathTraversalState {
public:
    enum class Action {
        TotalLength,
        VectorAtLength,
        SegmentAtLength,
    };

    PathTraversalState(Action, float desiredLength = 0);

public:
    bool processPathElement(PathElement::Type, const FloatPoint*);
    bool processPathElement(const PathElement& element) { return processPathElement(element.type, element.points); }

    Action action() const { return m_action; }
    void setAction(Action action) { m_action = action; }
    float desiredLength() const { return m_desiredLength; }
    void setDesiredLength(float desiredLength) { m_desiredLength = desiredLength; }

    // Traversing output -- should be read only
    bool success() const { return m_success; }
    float totalLength() const { return m_totalLength; }
    FloatPoint current() const { return m_current; }
    float normalAngle() const { return m_normalAngle; }

private:
    void closeSubpath();
    void moveTo(const FloatPoint&);
    void lineTo(const FloatPoint&);
    void quadraticBezierTo(const FloatPoint&, const FloatPoint&);
    void cubicBezierTo(const FloatPoint&, const FloatPoint&, const FloatPoint&);

    bool finalizeAppendPathElement();
    bool appendPathElement(PathElement::Type, const FloatPoint*);

private:
    Action m_action;
    bool m_success { false };

    FloatPoint m_current;
    FloatPoint m_start;

    float m_totalLength { 0 };
    float m_desiredLength { 0 };

    // For normal calculations
    FloatPoint m_previous;
    float m_normalAngle { 0 }; // degrees
    bool m_isZeroVector { false };
};
}

#endif
