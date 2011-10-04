/*
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 * Copyright (C) 2006, 2007, 2008 Apple Inc.  All rights reserved.
 * Copyright (C) 2007 Matt Lilek (pewtermoose@gmail.com).
 * Copyright (C) 2009 Joseph Pecoraro
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.elementDragStart = function(element, dividerDrag, elementDragEnd, event, cursor)
{
    if (WebInspector._elementDraggingEventListener || WebInspector._elementEndDraggingEventListener)
        WebInspector.elementDragEnd(event);

    WebInspector._elementDraggingEventListener = dividerDrag;
    WebInspector._elementEndDraggingEventListener = elementDragEnd;

    var targetDocument = event.target.ownerDocument;
    targetDocument.addEventListener("mousemove", dividerDrag, true);
    targetDocument.addEventListener("mouseup", elementDragEnd, true);

    targetDocument.body.style.cursor = cursor;

    event.preventDefault();
}

WebInspector.elementDragEnd = function(event)
{
    var targetDocument = event.target.ownerDocument;
    targetDocument.removeEventListener("mousemove", WebInspector._elementDraggingEventListener, true);
    targetDocument.removeEventListener("mouseup", WebInspector._elementEndDraggingEventListener, true);

    targetDocument.body.style.removeProperty("cursor");

    delete WebInspector._elementDraggingEventListener;
    delete WebInspector._elementEndDraggingEventListener;

    event.preventDefault();
}

WebInspector.animateStyle = function(animations, duration, callback)
{
    var interval;
    var complete = 0;
    var hasCompleted = false;

    const intervalDuration = (1000 / 30); // 30 frames per second.
    const animationsLength = animations.length;
    const propertyUnit = {opacity: ""};
    const defaultUnit = "px";

    function cubicInOut(t, b, c, d)
    {
        if ((t/=d/2) < 1) return c/2*t*t*t + b;
        return c/2*((t-=2)*t*t + 2) + b;
    }

    // Pre-process animations.
    for (var i = 0; i < animationsLength; ++i) {
        var animation = animations[i];
        var element = null, start = null, end = null, key = null;
        for (key in animation) {
            if (key === "element")
                element = animation[key];
            else if (key === "start")
                start = animation[key];
            else if (key === "end")
                end = animation[key];
        }

        if (!element || !end)
            continue;

        if (!start) {
            var computedStyle = element.ownerDocument.defaultView.getComputedStyle(element);
            start = {};
            for (key in end)
                start[key] = parseInt(computedStyle.getPropertyValue(key), 10);
            animation.start = start;
        } else
            for (key in start)
                element.style.setProperty(key, start[key] + (key in propertyUnit ? propertyUnit[key] : defaultUnit));
    }

    function animateLoop()
    {
        if (hasCompleted)
            return;
        
        // Advance forward.
        complete += intervalDuration;
        var next = complete + intervalDuration;

        // Make style changes.
        for (var i = 0; i < animationsLength; ++i) {
            var animation = animations[i];
            var element = animation.element;
            var start = animation.start;
            var end = animation.end;
            if (!element || !end)
                continue;

            var style = element.style;
            for (key in end) {
                var endValue = end[key];
                if (next < duration) {
                    var startValue = start[key];
                    var newValue = cubicInOut(complete, startValue, endValue - startValue, duration);
                    style.setProperty(key, newValue + (key in propertyUnit ? propertyUnit[key] : defaultUnit));
                } else
                    style.setProperty(key, endValue + (key in propertyUnit ? propertyUnit[key] : defaultUnit));
            }
        }

        // End condition.
        if (complete >= duration) {
            hasCompleted = true;
            clearInterval(interval);
            if (callback)
                callback();
        }
    }

    function forceComplete()
    {
        if (hasCompleted)
            return;

        complete = duration;
        animateLoop();
    }

    function cancel()
    {
        hasCompleted = true;
        clearInterval(interval);
    }

    interval = setInterval(animateLoop, intervalDuration);
    return {
        cancel: cancel,
        forceComplete: forceComplete
    };
}

