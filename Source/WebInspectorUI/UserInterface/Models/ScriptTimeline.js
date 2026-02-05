/*
 * Copyright (C) 2025 Devin Rousso <webkit@devinrousso.com>. All rights reserved.
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

WI.ScriptTimeline = class ScriptTimeline extends WI.Timeline
{
    // Public

    get targets()
    {
        return Array.from(this._callingContextTreesForTarget.keys());
    }

    get imported()
    {
        for (let target of this._callingContextTreesForTarget.keys()) {
            if (target instanceof WI.ImportedTarget)
                return true;
        }
        return false;
    }

    reset(suppressEvents)
    {
        this._callingContextTreesForTarget = new Map;

        super.reset(suppressEvents);
    }

    refresh()
    {
        this.dispatchEventToListeners(WI.Timeline.Event.Refresh);
    }

    callingContextTree(target, type)
    {
        return this._callingContextTreesForTarget.get(target)?.[type] || null;
    }

    updateCallingContextTrees(target, stackTraces, sampleDurations)
    {
        let targetAdded = false;

        let callingContextTrees = this._callingContextTreesForTarget.getOrInsertComputed(target, () => {
            targetAdded = true;

            let callingContextTrees = {};
            for (const type of Object.values(WI.CallingContextTree.Type))
                callingContextTrees[type] = new WI.CallingContextTree(target, type);
            return callingContextTrees;
        });

        for (let i = 0; i < stackTraces.length; i++) {
            for (const type of Object.values(WI.CallingContextTree.Type))
                callingContextTrees[type].updateTreeWithStackTrace(stackTraces[i], sampleDurations[i]);
        }

        if (targetAdded)
            this.dispatchEventToListeners(WI.ScriptTimeline.Event.TargetAdded, {target});
    }
};

WI.ScriptTimeline.Event = {
    Refreshed: "script-timeline-refreshed",
    TargetAdded: "script-timeline-target-added",
};
