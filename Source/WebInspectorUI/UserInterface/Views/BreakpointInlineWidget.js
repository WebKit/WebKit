/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

WI.BreakpointInlineWidget = class BreakpointInlineWidget
{
    constructor(breakpointOrSourceCodeLocation)
    {
        if (breakpointOrSourceCodeLocation instanceof WI.JavaScriptBreakpoint) {
            console.assert(breakpointOrSourceCodeLocation instanceof WI.JavaScriptBreakpoint, breakpointOrSourceCodeLocation);
            console.assert(!breakpointOrSourceCodeLocation.special, breakpointOrSourceCodeLocation);
            console.assert(breakpointOrSourceCodeLocation.resolved, breakpointOrSourceCodeLocation);

            this._breakpoint = breakpointOrSourceCodeLocation;
            this._sourceCodeLocation = this._breakpoint.sourceCodeLocation;
        } else {
            console.assert(breakpointOrSourceCodeLocation instanceof WI.SourceCodeLocation, breakpointOrSourceCodeLocation);

            this._sourceCodeLocation = breakpointOrSourceCodeLocation;
            this._breakpoint = null;
        }

        this._element = document.createElement("span");
        this._element.classList.add("inline-widget", "breakpoint");
        this._element.addEventListener("click", this._handleClick.bind(this));

        this._update();
    }

    // Public

    get element() { return this._element; }
    get sourceCodeLocation() { return this._sourceCodeLocation; }
    get breakpoint() { return this._breakpoint; }

    // Private

    _update()
    {
        this._element.classList.toggle("disabled", !this._breakpoint);
    }

    _handleClick(event)
    {
        if (this._breakpoint) {
            this._breakpoint.remove();
            this._breakpoint = null;
        } else {
            this._breakpoint = new WI.JavaScriptBreakpoint(this._sourceCodeLocation, {resolved: true});
            WI.debuggerManager.addBreakpoint(this._breakpoint);
        }

        this._update();
    }
};
