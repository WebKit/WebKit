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

WI.BlackboxedGroupTreeElement = class BlackboxedGroupTreeElement extends WI.GeneralTreeElement
{
    constructor(callFrames)
    {
        console.assert(Array.isArray(callFrames) && callFrames.length && callFrames.every((callFrame) => callFrame instanceof WI.CallFrame));

        const classNames = ["blackboxed-group"];
        const title = WI.UIString("Blackboxed", "Blackboxed @ Debugger Call Stack", "Part of the 'Blackboxed - %d call frames' label shown in the debugger call stack when paused instead of subsequent call frames that have been blackboxed.");
        let subtitle;
        if (callFrames.length === 1)
            subtitle = WI.UIString("%d call frame", "call frame @ Debugger Call Stack", "Part of the 'Blackboxed - %d call frame' label shown in the debugger call stack when paused instead of subsequent call frames that have been blackboxed.").format(callFrames.length);
        else
            subtitle = WI.UIString("%d call frames", "call frames @ Debugger Call Stack", "Part of the 'Blackboxed - %d call frames' label shown in the debugger call stack when paused instead of subsequent call frames that have been blackboxed.").format(callFrames.length);

        super(classNames, title, subtitle);

        this.selectable = false;
        this.toggleOnClick = true;
        this._callFrames = callFrames;
    }

    // Public

    expand()
    {
        let index = this.parent.children.indexOf(this);
        for (let i = this._callFrames.length - 1; i >= 0; --i)
            this.parent.insertChild(new WI.CallFrameTreeElement(this._callFrames[i]), index);

        this.parent.removeChild(this);
    }
};
