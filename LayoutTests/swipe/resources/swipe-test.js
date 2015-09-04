var eventQueue = {
    enqueueScrollEvent: function (x, y, phase) {
        this._queue.push(function () {
            log("scroll event (delta " + x + " " + y + ", phase '" + phase + "')");
            window.eventSender.mouseScrollByWithWheelAndMomentumPhases(x, y, phase, 'none');
        });
        this._processEventQueueSoon();
    },

    enqueueSwipeEvent: function (x, y, phase) {
        this._queue.push(function () {
            log("swipe event (delta " + x + " " + y + ", phase '" + phase + "')");
            window.eventSender.swipeGestureWithWheelAndMomentumPhases(x, y, phase, 'none');
        });
        this._processEventQueueSoon();
    },

    hasPendingEvents: function () {
        return this._queue.length != 0;
    },

    callAfterEventDispatch: function (callback) {
        var interval = setInterval(function () { 
        if (!eventQueue.hasPendingEvents()) {
            clearInterval(interval);
            callback();
        }
    }, 0);
    },

    _queue: [],

    _processEventQueue: function () {
        if (!this._queue.length)
            return;

        var item = this._queue.shift();
        item();
        this._processEventQueueSoon();
    },

    _processEventQueueSoon: function () {
        clearTimeout(this._processingTimeout);
        this._processingTimeout = setTimeout(this._processEventQueue.bind(this), 0);
    }
}

function shouldBe(expected, actual, message)
{
    if (expected != actual)
        log("Failure. " + message + " (expected: " + expected + ", actual: " + actual + ")");
}

function log(s)
{
    window.localStorage["swipeLogging"] += s + "<br/>";
}

function dumpLog()
{
    window.document.body.innerHTML = window.localStorage["swipeLogging"];
}

function testComplete()
{
    dumpLog();
    window.testRunner.notifyDone();
}

function initializeSwipeTest()
{
    window.localStorage["swipeLogging"] = "";
    testRunner.setNavigationGesturesEnabled(true);
    testRunner.clearBackForwardList();
}

function startMeasuringDuration(key)
{
    window.localStorage[key + "swipeStartTime"] = Date.now();
}

function measuredDurationShouldBeLessThan(key, timeInMS, message)
{
    var duration = Date.now() - window.localStorage[key + "swipeStartTime"];
    if (duration >= timeInMS)
        log("Failure. " + message + " (expected: " + timeInMS + ", actual: " + duration + ")");
}