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

WI.MemoryManager = class MemoryManager extends WI.Object
{
    constructor()
    {
        super();

        this._enabled = false;
        this._enabledTargets = new Set;
    }

    // Agent

    get domains() { return ["Memory"]; }

    activateExtraDomain(domain)
    {
        // COMPATIBILITY (iOS 14.0): Inspector.activateExtraDomains was removed in favor of a declared debuggable type

        console.assert(domain === "Memory");

        if (!this._enabled)
            return;

        for (let target of WI.targetManager.processLeaders)
            this._enableTarget(target);
    }

    // Public

    enable()
    {
        if (this._enabled)
            return;

        this._enabled = true;

        // Memory data and memory pressure cover a whole process, so only one target per process is enabled.
        WI.targetManager.addEventListener(WI.TargetManager.Event.ProcessLeaderChanged, this._handleProcessLeaderChanged, this);

        for (let target of WI.targetManager.processLeaders)
            this._enableTarget(target);
    }

    disable()
    {
        if (!this._enabled)
            return;

        WI.targetManager.removeEventListener(WI.TargetManager.Event.ProcessLeaderChanged, this._handleProcessLeaderChanged, this);

        for (let target of this._enabledTargets) {
            if (!target.isDestroyed)
                target.MemoryAgent.disable();
        }
        this._enabledTargets.clear();

        this._enabled = false;
    }

    // MemoryObserver

    memoryPressure(timestamp, protocolSeverity)
    {
        if (!this._enabled)
            return;

        let memoryPressureEvent = WI.MemoryPressureEvent.fromPayload(timestamp, protocolSeverity);
        this.dispatchEventToListeners(WI.MemoryManager.Event.MemoryPressure, {memoryPressureEvent});
    }

    // Private

    _enableTarget(target)
    {
        if (!target.hasDomain("Memory") || this._enabledTargets.has(target))
            return;

        this._enabledTargets.add(target);
        target.MemoryAgent.enable();
    }

    _handleProcessLeaderChanged(event)
    {
        let {previousLeader, leader} = event.data;

        if (previousLeader && this._enabledTargets.delete(previousLeader) && !previousLeader.isDestroyed)
            previousLeader.MemoryAgent.disable();

        if (leader)
            this._enableTarget(leader);
    }
};

WI.MemoryManager.Event = {
    MemoryPressure: "memory-manager-memory-pressure",
};
