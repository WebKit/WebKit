/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

WI.RecordingTraceDetailsSidebarPanel = class RecordingTraceDetailsSidebarPanel extends WI.DetailsSidebarPanel
{
    constructor()
    {
        super("recording-trace", WI.UIString("Trace"));

        this._recording = null;
        this._index = NaN;
    }

    // Static

    static disallowInstanceForClass()
    {
        return true;
    }

    // Public

    inspect(objects)
    {
        if (!(objects instanceof Array))
            objects = [objects];

        this.recording = objects.find((object) => object instanceof WI.Recording);

        return !!this._recording;
    }

    set recording(recording)
    {
        if (recording === this._recording)
            return;

        this._recording = recording;
        this._index = NaN;

        this.contentView.element.removeChildren();
    }

    updateActionIndex(index, context, options = {})
    {
        console.assert(!this._recording || (index >= 0 && index < this._recording.actions.length));
        if (!this._recording || index < 0 || index > this._recording.actions.length || index === this._index)
            return;

        this._index = index;

        this.contentView.element.removeChildren();

        let trace = this._recording.actions[this._index].trace;
        if (!trace.length) {
            let noTraceDataElement = this.contentView.element.appendChild(document.createElement("div"));
            noTraceDataElement.classList.add("no-trace-data");

            let noTraceDataMessageElement = noTraceDataElement.appendChild(document.createElement("div"));
            noTraceDataMessageElement.classList.add("message");
            noTraceDataMessageElement.textContent = WI.UIString("No Trace Data");
            return;
        }

        const showFunctionName = true;
        for (let callFrame of trace)
            this.contentView.element.appendChild(new WI.CallFrameView(callFrame, showFunctionName));
    }
};
