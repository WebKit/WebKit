/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

WI.TargetManager = class TargetManager extends WI.Object
{
    constructor()
    {
        super();

        this._targets = new Map;
        this._cachedTargetsList = null;
        this._seenPageTarget = false;
        this._transitionTimeoutIdentifier = undefined;

        // A provisional target moves to the end when it commits.
        this._targetsInCommitOrder = new Set;
        this._orphanedFrameTargets = new Set;
        this._processLeaders = new Map;
        this._processLeaderElectionScheduled = false;
    }

    // Static

    static processKeyForTarget(target)
    {
        switch (target.type) {
        case WI.TargetType.Frame:
            // COMPATIBILITY (macOS 27.3, iOS 27.3): `Target.TargetInfo.processIdentifier` did not exist yet.
            // WebKitLegacy frame targets do not have it either. In both cases the page target covers their process.
            return target.processIdentifier;

        case WI.TargetType.ServiceWorker:
        case WI.TargetType.WebPage:
        case WI.TargetType.Worker:
            return null;
        }

        // Without a process identifier, a target is a group of its own.
        return target.processIdentifier || target;
    }

    // Chooses one target per process to report process-wide data, such as CPU usage and memory. `targets` lists the
    // live targets, with committed targets in the order they committed. `previousLeaders` is the result of the
    // previous call. Targets in `orphanedFrameTargets` do not lead. Returns a Map from process key to leader.
    static electProcessLeaders(targets, previousLeaders = new Map, orphanedFrameTargets = new Set)
    {
        let groups = new Map;
        for (let target of targets) {
            if (target.isProvisional || orphanedFrameTargets.has(target))
                continue;

            let key = WI.TargetManager.processKeyForTarget(target);
            if (key === null)
                continue;

            let group = groups.get(key);
            if (!group) {
                group = {pageTargets: [], frameTargets: []};
                groups.set(key, group);
            }

            if (target.type === WI.TargetType.Frame)
                group.frameTargets.push(target);
            else
                group.pageTargets.push(target);
        }

        let leaders = new Map;
        for (let [key, {pageTargets, frameTargets}] of groups) {
            let previousLeader = previousLeaders.get(key) || null;

            // A live leader stays, unless a page target commits into a process that a frame target leads.
            if (pageTargets.length)
                leaders.set(key, pageTargets.includes(previousLeader) ? previousLeader : pageTargets[0]);
            else if (frameTargets.includes(previousLeader))
                leaders.set(key, previousLeader);
            else
                leaders.set(key, frameTargets[0]);
        }
        return leaders;
    }

    // When the page target that leads a process is removed, returns the frame targets of that process. They belong
    // to the removed page, so they do not lead.
    static frameTargetsOrphanedByRemovedTarget(removedTarget, targets, leaders)
    {
        if (removedTarget.type !== WI.TargetType.Page)
            return [];

        let key = WI.TargetManager.processKeyForTarget(removedTarget);
        if (leaders.get(key) !== removedTarget)
            return [];

        return Array.from(targets).filter((target) => target.type === WI.TargetType.Frame && WI.TargetManager.processKeyForTarget(target) === key);
    }

    // Target

    initializeTarget(target)
    {
        // COMPATIBILITY (iOS 13): Target.setPauseOnStart did not exist yet.
        if (target.hasCommand("Target.setPauseOnStart"))
            target.TargetAgent.setPauseOnStart(true);
    }

    // Public

    get targets()
    {
        if (!this._cachedTargetsList)
            this._cachedTargetsList = Array.from(this._targets.values()).filter((target) => !(target instanceof WI.MultiplexingBackendTarget));
        return this._cachedTargetsList;
    }

    get workerTargets()
    {
        return this.targets.filter((target) => target.type === WI.TargetType.Worker);
    }

    // Under site isolation the page target's debugger never attaches, so it can never pause.
    isDebuggerThreadTarget(target)
    {
        return target !== WI.pageTarget || !this.targets.some((target) => target instanceof WI.FrameTarget);
    }

    get allTargets()
    {
        return Array.from(this._targets.values());
    }

    get processLeaders()
    {
        return Array.from(this._processLeaders.values()).filter((leader) => !leader.isDestroyed);
    }

    processLeaderForTarget(target)
    {
        let key = WI.TargetManager.processKeyForTarget(target);
        if (key === null)
            return null;

        // A removed leader stays in the map until the next election, so that election reports it as the previous leader.
        let leader = this._processLeaders.get(key);
        return leader && !leader.isDestroyed ? leader : null;
    }

    isProcessLeader(target)
    {
        return this.processLeaderForTarget(target) === target;
    }

    targetForIdentifier(targetId)
    {
        if (!targetId)
            return null;

        for (let target of this._targets.values()) {
            if (target.identifier === targetId)
                return target;
        }

        return null;
    }

    addTarget(target)
    {
        console.assert(target);
        console.assert(!this._targets.has(target.identifier));

        this._cachedTargetsList = null;
        this._targets.set(target.identifier, target);
        this._targetsInCommitOrder.add(target);
        this._scheduleProcessLeaderElection();

        this.dispatchEventToListeners(WI.TargetManager.Event.TargetAdded, {target});
    }

    removeTarget(target)
    {
        console.assert(target);
        console.assert(target !== WI.mainTarget);

        this._cachedTargetsList = null;
        this._targets.delete(target.identifier);
        this._targetsInCommitOrder.delete(target);
        this._orphanedFrameTargets.delete(target);
        for (let frameTarget of WI.TargetManager.frameTargetsOrphanedByRemovedTarget(target, this._targetsInCommitOrder, this._processLeaders))
            this._orphanedFrameTargets.add(frameTarget);
        target.destroy();
        this._scheduleProcessLeaderElection();

        this.dispatchEventToListeners(WI.TargetManager.Event.TargetRemoved, {target});
    }

    createMultiplexingBackendTarget()
    {
        console.assert(WI.sharedApp.debuggableType === WI.DebuggableType.WebPage);

        let target = new WI.MultiplexingBackendTarget;
        target.initialize();

        this._initializeBackendTarget(target);

        // Add the target without dispatching an event.
        this._targets.set(target.identifier, target);
    }

    createDirectBackendTarget()
    {
        console.assert(WI.sharedApp.debuggableType !== WI.DebuggableType.WebPage);

        let target = new WI.DirectBackendTarget;
        target.initialize();

        this._initializeBackendTarget(target);

        if (WI.sharedApp.debuggableType === WI.DebuggableType.ITML || WI.sharedApp.debuggableType === WI.DebuggableType.Page)
            this._initializePageTarget(target);

        this.addTarget(target);
    }

    // TargetObserver

    targetCreated(parentTarget, targetInfo)
    {
        let connection = new InspectorBackend.TargetConnection(parentTarget, targetInfo.targetId);
        let subTarget = this._createTarget(parentTarget, targetInfo, connection);
        this._checkAndHandlePageTargetTransition(subTarget);
        subTarget.initialize();
        this.addTarget(subTarget);
    }

    didCommitProvisionalTarget(parentTarget, previousTargetId, newTargetId)
    {
        this.targetDestroyed(previousTargetId);
        let target = this._targets.get(newTargetId);
        console.assert(target);
        if (!target)
            return;

        target.didCommitProvisionalTarget();
        this._targetsInCommitOrder.delete(target);
        this._targetsInCommitOrder.add(target);
        this._scheduleProcessLeaderElection();
        this._checkAndHandlePageTargetTransition(target);
        target.connection.dispatchProvisionalMessages();

        this.dispatchEventToListeners(WI.TargetManager.Event.DidCommitProvisionalTarget, {previousTargetId, target});
    }

    targetDestroyed(targetId)
    {
        let target = this._targets.get(targetId);
        if (!target)
            return;

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=312386 SI Move cleanup of children workers to backend
        if (target.type === WI.TargetType.Frame) {
            for (let workerTarget of this.workerTargets) {
                if (workerTarget.parentTarget === target)
                    WI.workerManager.workerTerminated(workerTarget.identifier);
            }
        }

        this._checkAndHandlePageTargetTermination(target);
        this.removeTarget(target);
    }

    dispatchMessageFromTarget(targetId, message)
    {
        let target = this._targets.get(targetId);
        console.assert(target);
        if (!target)
            return;

        if (target.isProvisional)
            target.connection.addProvisionalMessage(message);
        else
            target.connection.dispatch(message);
    }

    // Private

    _createTarget(parentTarget, targetInfo, connection)
    {
        // COMPATIBILITY (iOS 13.0): `Target.TargetInfo.isProvisional` and `Target.TargetInfo.isPaused` did not exist yet.
        let {targetId, type, isProvisional, isPaused, processIdentifier} = targetInfo;

        switch (type) {
        case InspectorBackend.Enum.Target.TargetInfoType.Page:
            return new WI.PageTarget(parentTarget, targetId, WI.UIString("Page"), connection, {isProvisional, isPaused, processIdentifier});
        // `Target.TargetInfo` has no URL.
        case InspectorBackend.Enum.Target.TargetInfoType.Worker:
            return new WI.WorkerTarget(parentTarget, targetId, null, WI.UIString("Worker"), connection, {isPaused});
        case "serviceworker": // COMPATIBILITY (iOS 13): "serviceworker" was renamed to "service-worker".
        case InspectorBackend.Enum.Target.TargetInfoType.ServiceWorker:
            return new WI.WorkerTarget(parentTarget, targetId, null, WI.UIString("ServiceWorker"), connection, {isPaused});
        case InspectorBackend.Enum.Target.TargetInfoType.Frame:
            // FIXME: <https://webkit.org/b/298977> Consider setting a more meaningful name for the frame target.
            return new WI.FrameTarget(parentTarget, targetId, WI.UIString("Frame"), connection, {isProvisional, isPaused, processIdentifier});
        }

        throw "Unknown Target type: " + type;
    }

    _checkAndHandlePageTargetTransition(target)
    {
        if (target.type !== WI.TargetType.Page)
            return;

        if (target.isProvisional)
            return;

        // First page target.
        if (!WI.pageTarget && !this._seenPageTarget) {
            this._seenPageTarget = true;
            this._initializePageTarget(target);
            return;
        }

        // Transitioning page target.
        this._transitionPageTarget(target);
    }

    _checkAndHandlePageTargetTermination(target)
    {
        if (target.type !== WI.TargetType.Page)
            return;

        if (target.isProvisional)
            return;

        console.assert(target === WI.pageTarget);
        console.assert(this._seenPageTarget);

        // Terminating the page target.
        this._terminatePageTarget(target);

        // Ensure we transition in a reasonable amount of time, otherwise close.
        const timeToTransition = 2000;
        clearTimeout(this._transitionTimeoutIdentifier);
        this._transitionTimeoutIdentifier = setTimeout(() => {
            this._transitionTimeoutIdentifier = undefined;
            if (WI.pageTarget)
                return;
            if (WI.isEngineeringBuild)
                throw new Error("Error: No new pageTarget some time after last page target was terminated. Failed transition?");
            WI.close();
        }, timeToTransition);
    }

    _initializeBackendTarget(target)
    {
        console.assert(!WI.mainTarget);

        WI.backendTarget = target;

        this._resetMainExecutionContext();

        WI._backendTargetAvailablePromise.resolve();
    }

    _initializePageTarget(target)
    {
        console.assert(WI.sharedApp.isWebDebuggable() || WI.sharedApp.debuggableType === WI.DebuggableType.ITML);
        console.assert(target.type === WI.TargetType.Page || target instanceof WI.DirectBackendTarget);

        WI.pageTarget = target;

        this._resetMainExecutionContext();

        WI._pageTargetAvailablePromise.resolve();
        WI._pageTargetAvailablePromise.settled = true;
    }

    _transitionPageTarget(target)
    {
        console.assert(!WI.pageTarget);
        console.assert(WI.sharedApp.debuggableType === WI.DebuggableType.WebPage);
        console.assert(target.type === WI.TargetType.Page);

        WI.pageTarget = target;

        this._resetMainExecutionContext();

        // Actions to transition the page target.
        WI.notifications.dispatchEventToListeners(WI.Notification.TransitionPageTarget);
        WI.domManager.transitionPageTarget();
        WI.networkManager.transitionPageTarget();
        WI.timelineManager.transitionPageTarget();
    }

    _terminatePageTarget(target)
    {
        console.assert(WI.pageTarget);
        console.assert(WI.pageTarget === target);
        console.assert(WI.sharedApp.debuggableType === WI.DebuggableType.WebPage);

        // Remove any Worker targets associated with this page.
        for (let workerTarget of this.workerTargets)
            WI.workerManager.workerTerminated(workerTarget.identifier);

        WI.pageTarget = null;
    }

    _resetMainExecutionContext()
    {
        if (WI.mainTarget instanceof WI.MultiplexingBackendTarget)
            return;

        if (WI.mainTarget.executionContext)
            WI.runtimeManager.activeExecutionContext = WI.mainTarget.executionContext;
    }

    _scheduleProcessLeaderElection()
    {
        // Elect once per task, after its target changes. `targetCreated` initializes a target before it adds it.
        if (this._processLeaderElectionScheduled)
            return;

        this._processLeaderElectionScheduled = true;
        queueMicrotask(() => {
            this._processLeaderElectionScheduled = false;
            this._electProcessLeaders();
        });
    }

    _electProcessLeaders()
    {
        let previousLeaders = this._processLeaders;
        this._processLeaders = WI.TargetManager.electProcessLeaders(this._targetsInCommitOrder, previousLeaders, this._orphanedFrameTargets);

        for (let key of new Set([...previousLeaders.keys(), ...this._processLeaders.keys()])) {
            let previousLeader = previousLeaders.get(key) || null;
            let leader = this._processLeaders.get(key) || null;
            if (leader !== previousLeader)
                this.dispatchEventToListeners(WI.TargetManager.Event.ProcessLeaderChanged, {previousLeader, leader});
        }
    }
};

WI.TargetManager.Event = {
    TargetAdded: "target-manager-target-added",
    TargetRemoved: "target-manager-target-removed",
    DidCommitProvisionalTarget: "target-manager-provisional-target-committed",
    ProcessLeaderChanged: "target-manager-process-leader-changed",
};
