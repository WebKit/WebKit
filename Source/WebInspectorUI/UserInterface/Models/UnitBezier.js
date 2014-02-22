/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.UnitBezier = function(x1, y1, x2, y2)
{
    WebInspector.Object.call(this);

    // Calculate the polynomial coefficients, implicit first and last control points are (0,0) and (1,1).
    this._cx = 3.0 * x1;
    this._bx = 3.0 * (x2 - x1) - this._cx;
    this._ax = 1.0 - this._cx - this._bx;

    this._cy = 3.0 * y1;
    this._by = 3.0 * (y2 - y1) - this._cy;
    this._ay = 1.0 - this._cy - this._by;
};

WebInspector.UnitBezier.prototype = {
    constructor: WebInspector.UnitBezier,

    // Public

    solve: function(x, epsilon)
    {
        return this._sampleCurveY(this._solveCurveX(x, epsilon));
    },

    // Private

    _sampleCurveX: function(t)
    {
        // `ax t^3 + bx t^2 + cx t' expanded using Horner's rule.
        return ((this._ax * t + this._bx) * t + this._cx) * t;
    },

    _sampleCurveY: function(t)
    {
        return ((this._ay * t + this._by) * t + this._cy) * t;
    },

    _sampleCurveDerivativeX: function(t)
    {
        return (3.0 * this._ax * t + 2.0 * this._bx) * t + this._cx;
    },

    // Given an x value, find a parametric value it came from.
    _solveCurveX: function(x, epsilon)
    {
        var t0, t1, t2, x2, d2, i;

        // First try a few iterations of Newton's method -- normally very fast.
        for (t2 = x, i = 0; i < 8; i++) {
            x2 = this._sampleCurveX(t2) - x;
            if (Math.abs(x2) < epsilon)
                return t2;
            d2 = this._sampleCurveDerivativeX(t2);
            if (Math.abs(d2) < 1e-6)
                break;
            t2 = t2 - x2 / d2;
        }

        // Fall back to the bisection method for reliability.
        t0 = 0.0;
        t1 = 1.0;
        t2 = x;

        if (t2 < t0)
            return t0;
        if (t2 > t1)
            return t1;

        while (t0 < t1) {
            x2 = this._sampleCurveX(t2);
            if (Math.abs(x2 - x) < epsilon)
                return t2;
            if (x > x2)
                t0 = t2;
            else
                t1 = t2;
            t2 = (t1 - t0) * 0.5 + t0;
        }

        // Failure.
        return t2;
    }
};
