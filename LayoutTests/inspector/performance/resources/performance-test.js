var initialize_TimeTracker = function() {

InspectorTest.runPerformanceTest = function(perfTest, executeTime, callback)
{
    var Timer = function(test, callback)
    {
        this._callback = callback;
        this._test = test;
        this._times = {};
        this._testStartTime = new Date();
    }

    Timer.prototype = {
        start: function(name)
        {
            return {name: name, startTime: new Date()};
        },

        finish: function(cookie)
        {
            var endTime = new Date();
            if (!this._times[cookie.name])
                this._times[cookie.name] = [];
            this._times[cookie.name].push(endTime - cookie.startTime);
        },

        done: function()
        {
            var time = new Date();
            if (time - this._testStartTime < executeTime)
                this._runTest();
            else {
                this._dump();
                if (this._callback)
                    this._callback();
                else
                    InspectorTest.completeTest();
            }
        },

        _runTest: function()
        {
            if (this._guard) {
                setTimeout(this._runTest.bind(this), 0);
                return;
            }

            this._guard = true;
            var safeTest = InspectorTest.safeWrap(this._test);
            safeTest(this);
            this._guard = false;
        },

        _dump: function()
        {
            for (var testName in this._times) {
                var samples = this._times[testName];
                var stripNResults = Math.floor(samples.length / 10);
                samples.sort(function(a, b) { return a - b; });
                var sum = 0;
                for (var i = stripNResults; i < samples.length - stripNResults; ++i)
                    sum += samples[i];
                InspectorTest.addResult("* " + testName + ": " + Math.floor(sum / (samples.length - stripNResults * 2)));
                InspectorTest.addResult(testName + " min/max/count: " + samples[0] + "/" + samples[samples.length-1] + "/" + samples.length);
            }
        }
    }

    var timer = new Timer(perfTest, callback);
    timer._runTest();
}

InspectorTest.addBackendResponseSniffer = function(object, methodName, override, opt_sticky)
{
    var originalMethod = InspectorTest.override(object, methodName, backendCall, opt_sticky);
    function backendCall()
    {
        var args = Array.prototype.slice.call(arguments);
        var callback = (args.length && typeof args[args.length - 1] === "function") ? args.pop() : 0;
        args.push(function() {
            callback.apply(null, arguments);
            override.apply(null, arguments);
        });
        originalMethod.apply(object, args);
    }
}


}
