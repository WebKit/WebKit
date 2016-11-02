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

WebInspector.DebuggerData = class DebuggerData extends WebInspector.Object
{
    constructor(target)
    {
        super();

        console.assert(target instanceof WebInspector.Target);

        this._target = target;

        this._callFrames = [];
        this._pauseReason = null;
        this._pauseData = null;

        this._scriptIdMap = new Map;
        this._scriptContentIdentifierMap = new Map;
    }

    // Public

    get target() { return this._target; }
    get callFrames() { return this._callFrames; }
    get pauseReason() { return this._pauseReason; }
    get pauseData() { return this._pauseData; }

    get scripts()
    {
        return Array.from(this._scriptIdMap.values());
    }

    scriptForIdentifier(id)
    {
        return this._scriptIdMap.get(id);
    }

    scriptsForURL(url)
    {
        return this._scriptContentIdentifierMap.get(url) || [];
    }

    // Protected (Called by DebuggerManager)

    reset()
    {
        this._scriptIdMap.clear();
    }

    addScript(script)
    {
        this._scriptIdMap.set(script.id, script);

        if (script.contentIdentifier) {
            let scripts = this._scriptContentIdentifierMap.get(script.contentIdentifier);
            if (!scripts) {
                scripts = [];
                this._scriptContentIdentifierMap.set(script.contentIdentifier, scripts);
            }
            scripts.push(script);
        }
    }

    updateForPause(callFrames, pauseReason, pauseData)
    {
        this._callFrames = callFrames;
        this._pauseReason = pauseReason;
        this._pauseData = pauseData;
    }

    updateForResume()
    {
        this._callFrames = [];
        this._pauseReason = null;
        this._pauseData = null;
    }
};
