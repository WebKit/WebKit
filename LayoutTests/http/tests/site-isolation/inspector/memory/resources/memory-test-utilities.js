TestPage.registerInitializer(() => {
    window.MemoryTest = {};

    // Keep every Memory event with the target that sent it. The events do not reach TimelineManager or MemoryManager,
    // so that starting a target directly does not start a Timeline capture.
    let events = [];
    let eventListeners = [];
    for (let name of ["memoryPressure", "trackingStart", "trackingUpdate", "trackingComplete"]) {
        WI.MemoryObserver.prototype[name] = function(...args) {
            let event = {target: this._target, name, args};
            events.push(event);
            for (let listener of eventListeners.slice()) {
                if (listener.target === event.target && listener.name === name && MemoryTest.events(event.target, name).length >= listener.count) {
                    eventListeners.remove(listener);
                    listener.resolve(event);
                }
            }
        };
    }

    MemoryTest.clearEvents = function() {
        events = [];
    };

    // Returns the events with the given name from the target, or from every target if there is none.
    MemoryTest.events = function(target, name) {
        return events.filter((event) => (!target || event.target === target) && event.name === name);
    };

    // Resolves with the `count`-th event with the given name from the target since the events were last cleared.
    MemoryTest.awaitEvent = function(target, name, count = 1) {
        let existing = MemoryTest.events(target, name);
        if (existing.length >= count)
            return Promise.resolve(existing[count - 1]);

        return new Promise((resolve) => {
            eventListeners.push({target, name, count, resolve});
        });
    };

    MemoryTest.targetsThatSentEvents = function() {
        return new Set(events.map((event) => event.target));
    };

    MemoryTest.evaluateInTarget = async function(target, expression) {
        let response = await target.RuntimeAgent.evaluate.invoke({expression, objectGroup: "test", returnByValue: true});
        if (response.wasThrown)
            throw new Error(`Evaluating "${expression}" threw: ${response.result.description}`);
        return response.result.value;
    };

    // Resolves once each target answered a command, so the events that the targets sent before have arrived.
    MemoryTest.awaitRoundTrip = function(targets) {
        return Promise.all(targets.map((target) => MemoryTest.evaluateInTarget(target, "0")));
    };

    // Resolves with the error message of the command, or null if the command succeeded.
    MemoryTest.commandError = async function(promise) {
        try {
            await promise;
            return null;
        } catch (error) {
            return error.message;
        }
    };

    MemoryTest.logUpdate = function(update) {
        InspectorTest.expectEqual(typeof update.timestamp, "number", "The update should have a timestamp.");
        let types = update.categories.map((category) => category.type).sort();
        InspectorTest.log("Categories: " + types.join(", "));
        InspectorTest.expectThat(update.categories.every((category) => typeof category.size === "number" && category.size >= 0), "Every category should have a size that is not negative.");
        InspectorTest.expectGreaterThan(update.categories.reduce((total, category) => total + category.size, 0), 0, "The total size should be more than zero.");
    };

    // Tracks memory on the target until it sent one update, and checks the events.
    MemoryTest.trackOnTarget = async function(target) {
        MemoryTest.clearEvents();

        let startPromise = MemoryTest.awaitEvent(target, "trackingStart");
        let updatePromise = MemoryTest.awaitEvent(target, "trackingUpdate");
        await target.MemoryAgent.startTracking();
        let [startTimestamp] = (await startPromise).args;
        let [update] = (await updatePromise).args;

        let completePromise = MemoryTest.awaitEvent(target, "trackingComplete");
        await target.MemoryAgent.stopTracking();
        let [completeTimestamp] = (await completePromise).args;

        InspectorTest.pass("The target sent trackingStart, trackingUpdate and trackingComplete.");
        InspectorTest.expectEqual(typeof startTimestamp, "number", "trackingStart should have a timestamp.");
        InspectorTest.expectGreaterThanOrEqual(completeTimestamp, startTimestamp, "trackingComplete should not be earlier than trackingStart.");
        MemoryTest.logUpdate(update);
        InspectorTest.expectEqual(MemoryTest.events(target, "trackingStart").length, 1, "The target should send one trackingStart.");
        InspectorTest.expectEqual(MemoryTest.events(target, "trackingComplete").length, 1, "The target should send one trackingComplete.");
    };
});
