/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

class AnimationTest
{

    // Expects a dictionary with an Element (target) and a Function (styleExtractor) used to extract
    // the recorded value from a computed style.
    constructor(options)
    {
        this._records = [];
        this._target = options.target;
        this._styleExtractor = options.styleExtractor;
    }

    // Public

    // Returns the Animation object. Currently, this expects a single animation is running on the
    // provided element.
    get animation()
    {
        return this._target.getAnimations()[0];
    }

    // Returns the requested value from the computed style using the provided style extractor.
    get value()
    {
        return this._styleExtractor(getComputedStyle(this._target));
    }

    // Returns the requested value from the computed style after waiting a certain delay.
    // This method runs independently of the animation play state and waits at a minimum
    // for an animation frame to have elapsed.
    async valueAfterWaitingFor(delay)
    {
        await Promise.all([
            new Promise(requestAnimationFrame),
            new Promise(resolve => setTimeout(resolve, delay))
        ]);
        return this.value;
    }

    // Records the requested value from the computed style after a given delay has elapsed while the
    // animation is running. The record stored will be checked when checkRecordedValues() is called.
    async recordValueAfterRunningFor(delay)
    {
        const animation = this.animation;
        await animation.ready;

        const targetTime = animation.currentTime + delay;
        await this._tickUntil(elapsedTime => animation.currentTime >= targetTime);

        const value = this.value;
        this._records.push({
            currentTime: animation.currentTime,
            value: value
        });

        return value;
    }

    // Check that all requested values recorded using recordValueAfterRunningFor() match the same values
    // when the animation is paused and seeked at the same animation currentTime as when the record was made.
    checkRecordedValues()
    {
        const animation = this.animation;

        // Reset the animation.
        animation.cancel();

        // Now seek to each of the recorded times and assert the value.
        this._records.forEach((record, index) => {
            animation.currentTime = record.currentTime;
            assert_equals(record.value, this.value, `Recorded value #${index} is correct.`);
        });
    }

    // Private

    // Wait until the provided condition is true using animation frames.
    _tickUntil(condition)
    {
        return new Promise(resolve => {
            if (!this._startTime)
                this._startTime = performance.now();

            const tick = timestamp => {
                const elapsedTime = timestamp - this._startTime;
                if (condition(elapsedTime)) {
                    delete this._startTime;
                    resolve();
                }
                requestAnimationFrame(tick);
            }
            requestAnimationFrame(tick);
        });
    }

}
