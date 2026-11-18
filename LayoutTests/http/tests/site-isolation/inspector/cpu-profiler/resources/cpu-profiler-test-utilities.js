TestPage.registerInitializer(() => {
    window.CPUProfilerTest = {};

    // Keep every CPUProfiler event with the target that sent it. Unless `forwardEvents` is set, the events do not
    // reach TimelineManager, so that starting a target directly does not start a Timeline capture.
    CPUProfilerTest.forwardEvents = false;

    let events = [];
    let eventListeners = [];
    for (let name of ["trackingStart", "trackingUpdate", "trackingComplete"]) {
        let original = WI.CPUProfilerObserver.prototype[name];
        WI.CPUProfilerObserver.prototype[name] = function(...args) {
            let event = {target: this._target, name, data: args[0]};
            events.push(event);
            for (let listener of eventListeners.slice()) {
                if (listener.target === event.target && listener.name === name && CPUProfilerTest.events(event.target, name).length >= listener.count) {
                    eventListeners.remove(listener);
                    listener.resolve(event);
                }
            }

            if (CPUProfilerTest.forwardEvents)
                return original.apply(this, args);
        };
    }

    CPUProfilerTest.clearEvents = function() {
        events = [];
    };

    // Returns the events with the given name from the target, or from every target if there is none.
    CPUProfilerTest.events = function(target, name) {
        return events.filter((event) => (!target || event.target === target) && event.name === name);
    };

    // Resolves with the `count`-th event with the given name from the target since the events were last cleared.
    CPUProfilerTest.awaitEvent = function(target, name, count = 1) {
        let existing = CPUProfilerTest.events(target, name);
        if (existing.length >= count)
            return Promise.resolve(existing[count - 1]);

        return new Promise((resolve) => {
            eventListeners.push({target, name, count, resolve});
        });
    };

    // Tracks CPU usage on the target until it sent `updateCount` updates, and resolves with those updates.
    CPUProfilerTest.trackOnTarget = async function(target, updateCount) {
        CPUProfilerTest.clearEvents();

        let startPromise = CPUProfilerTest.awaitEvent(target, "trackingStart");
        let updatePromise = CPUProfilerTest.awaitEvent(target, "trackingUpdate", updateCount);
        await target.CPUProfilerAgent.startTracking();
        await startPromise;
        await updatePromise;

        let completePromise = CPUProfilerTest.awaitEvent(target, "trackingComplete");
        await target.CPUProfilerAgent.stopTracking();
        await completePromise;

        return CPUProfilerTest.events(target, "trackingUpdate").slice(0, updateCount).map((event) => event.data);
    };

    CPUProfilerTest.targetsThatSentEvents = function() {
        return new Set(events.map((event) => event.target));
    };

    CPUProfilerTest.hasThreadForTarget = function(update, target) {
        return (update.threads || []).some((thread) => thread.targetId === target.identifier);
    };

    CPUProfilerTest.mainThreadCount = function(update) {
        return (update.threads || []).filter((thread) => thread.type === InspectorBackend.Enum.CPUProfiler.ThreadInfoType.Main).length;
    };

    // Resolves with the worker target whose parent is the given target.
    CPUProfilerTest.awaitWorkerTarget = function(parentTarget) {
        let existing = WI.targets.find((target) => target.type === WI.TargetType.Worker && target.parentTarget === parentTarget);
        if (existing)
            return Promise.resolve(existing);

        return new Promise((resolve) => {
            let listener = WI.targetManager.addEventListener(WI.TargetManager.Event.TargetAdded, (event) => {
                let {target} = event.data;
                if (target.type !== WI.TargetType.Worker || target.parentTarget !== parentTarget)
                    return;

                WI.targetManager.removeEventListener(WI.TargetManager.Event.TargetAdded, listener);
                resolve(target);
            });
        });
    };
});
