/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

function SpringSolver(mass, stiffness, damping, initialVelocity)
{
    this.m_w0 = Math.sqrt(stiffness / mass);
    this.m_zeta = damping / (2 * Math.sqrt(stiffness * mass));

    if (this.m_zeta < 1) {
        // Under-damped.
        this.m_wd = this.m_w0 * Math.sqrt(1 - this.m_zeta * this.m_zeta);
        this.m_A = 1;
        this.m_B = (this.m_zeta * this.m_w0 + -initialVelocity) / this.m_wd;
    } else {
        // Critically damped (ignoring over-damped case for now).
        this.m_wd = 0;
        this.m_A = 1;
        this.m_B = -initialVelocity + this.m_w0;
    }
}

SpringSolver.prototype.solve = function (t)
{
    if (this.m_zeta < 1) {
        // Under-damped
        t = Math.exp(-t * this.m_zeta * this.m_w0) * (this.m_A * Math.cos(this.m_wd * t) + this.m_B * Math.sin(this.m_wd * t));
    } else {
        // Critically damped (ignoring over-damped case for now).
        t = (this.m_A + this.m_B * t) * Math.exp(-t * this.m_w0);
    }

    // Map range from [1..0] to [0..1].
    return 1 - t;
};
