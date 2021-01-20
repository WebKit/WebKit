test(function(t) {
    assert_greater_than(performance.getEntriesByType("resource").length, 0, "context should already have some resources");
    performance.clearResourceTimings();
    assert_equals(performance.getEntriesByType("resource").length, 0, "clearResourceTimings should have cleared the buffer");
}, "performance.clearResourceTimings clears the buffer");

test(function(t) {
    performance.clearResourceTimings();
    performance.clearResourceTimings();
    performance.clearResourceTimings();
    assert_equals(performance.getEntriesByType("resource").length, 0, "clearResourceTimings should have cleared the buffer");
}, "performance.clearResourceTimings called multiple times is okay");

promise_test(function(t) {
    let bufferFullEventDispatched = false;
    performance.onresourcetimingbufferfull = function(event) {
        bufferFullEventDispatched = true;
    };

    return loadResources(3).then(function(entries) {
        assert_equals(entries.length, 3, "context should have observed 3 resources");
        assert_equals(performance.getEntriesByType("resource").length, 3, "context should have loaded 4 resources");
        assert_false(bufferFullEventDispatched, "context should not have dispatched buffer full event");
    });
}, "resourcetimingbufferfull event should not trigger for a small number of resources (browser default buffer size)", {timeout: 3000});

promise_test(function(t) {
    restoreEnvironmentToCleanState();
    assert_equals(performance.getEntriesByType("resource").length, 0, "context should have no resources");

    performance.setResourceTimingBufferSize(1);

    return loadResources(3).then(function(entries) {
        assert_equals(entries.length, 3, "context should have observed 3 resources");
        assert_equals(performance.getEntriesByType("resource").length, 1, "context global buffer should be full at 1 resource");
    });
}, "PerformanceObserver sees all resource entries even after buffer is full", {timeout: 3000});

promise_test(function(t) {
    restoreEnvironmentToCleanState();

    performance.setResourceTimingBufferSize(3);

    let bufferFullEventDispatched = false;
    performance.onresourcetimingbufferfull = function(event) {
        bufferFullEventDispatched = true;
    };

    return loadResources(2).then(function(entries) {
        assert_equals(entries.length, 2, "context should have observed 3 resources");
        assert_equals(performance.getEntriesByType("resource").length, 2, "context global buffer should have 2 resources when full");
        assert_false(bufferFullEventDispatched, "context should not have dispatched buffer full event");
    });
}, "resourcetimingbufferfull event should not trigger if less than the BufferSizeLimit number of resources are added to the buffer", {timeout: 3000});

promise_test(function(t) {
    restoreEnvironmentToCleanState();

    performance.setResourceTimingBufferSize(3);

    let bufferFullEventDispatched = false;
    performance.onresourcetimingbufferfull = function(event) {
        bufferFullEventDispatched = true;
    };

    return loadResources(3).then(function(entries) {
        assert_equals(entries.length, 3, "context should have observed 3 resources");
        assert_equals(performance.getEntriesByType("resource").length, 3, "context global buffer should be full at 3 resources");
        assert_false(bufferFullEventDispatched, "context should not have dispatched buffer full event");
    });
}, "resourcetimingbufferfull event should not trigger if exactly the BufferSizeLimit number of resources are added to the buffer", {timeout: 3000});

promise_test(function(t) {
    restoreEnvironmentToCleanState();
    assert_equals(performance.getEntriesByType("resource").length, 0, "context should have no resources");

    performance.setResourceTimingBufferSize(3);

    let bufferFullEventDispatchedCount = 0;
    performance.onresourcetimingbufferfull = function(event) {
        bufferFullEventDispatchedCount++;
    };

    return loadResources(5).then(function(entries) {
        assert_equals(entries.length, 5, "context should have observed 3 resources");
        assert_equals(performance.getEntriesByType("resource").length, 3, "context global buffer should be full at 3 resources");
        assert_equals(bufferFullEventDispatchedCount, 1, "context should have dispatched the buffer full event just once");
    });
}, "resourcetimingbufferfull event should only trigger once if more than the BufferSizeLimit number of resources are added to the buffer", {timeout: 3000});

promise_test(function(t) {
    restoreEnvironmentToCleanState();

    performance.setResourceTimingBufferSize(1);

    let bufferFullEvent = null;
    performance.onresourcetimingbufferfull = function(event) {
        bufferFullEvent = event;
    };

    return loadResources(2).then(function(entries) {
        assert_equals(entries.length, 2, "context should have observed 1 resource");
        assert_equals(performance.getEntriesByType("resource").length, 1, "context global buffer should be full at 1 resource");
        assert_equals(bufferFullEvent.target, performance, "event should dispatch at the performance object");
        assert_false(bufferFullEvent.bubbles, "event should not bubble");
    });
}, "resourcetimingbufferfull event properties", {timeout: 3000});

promise_test(function(t) {
    restoreEnvironmentToCleanState();

    performance.setResourceTimingBufferSize(100);

    let bufferFullEventDispatched = false;
    performance.onresourcetimingbufferfull = function(event) {
        bufferFullEventDispatched = true;
    };

    return loadResources(3).then(function(entries) {
        assert_equals(entries.length, 3, "context should have observed 1 resource");
        assert_equals(performance.getEntriesByType("resource").length, 3, "context global buffer should be full at 3 resources");
        assert_false(bufferFullEventDispatched, "event should not have been dispatched");

        let bufferFullEventDispatchedWithSizeChange = false;
        performance.onresourcetimingbufferfull = function(event) {
            bufferFullEventDispatchedWithSizeChange = true;
        };

        performance.setResourceTimingBufferSize(1);
        assert_false(bufferFullEventDispatchedWithSizeChange, "event should have been dispatched when size limit changed");
        assert_equals(performance.getEntriesByType("resource").length, 3, "context global buffer should still have 3 resources"); 
    });
}, "performance.setResourceTimingBufferSize set to value less than current BufferSize should not clear existing entries", {timeout: 3000});

// FIXME: Clarify behavior of setResourceTimingBufferSize
// <https://github.com/w3c/resource-timing/issues/96>
