// FIXME: Use the real promise if available.
// FIXME: Make sure this interface is compatible with the real Promise.
function SimplePromise() {
    this._chainedPromise = null;
    this._callback = null;
}

SimplePromise.prototype.then = function (callback) {
    if (this._callback)
        throw "SimplePromise doesn't support multiple calls to then";
    this._callback = callback;
    this._chainedPromise = new SimplePromise;
    
    if (this._resolved)
        this.resolve(this._resolvedValue);

    return this._chainedPromise;
}

SimplePromise.prototype.resolve = function (value) {
    if (!this._callback) {
        this._resolved = true;
        this._resolvedValue = value;
        return;
    }

    var result = this._callback(value);
    if (result instanceof SimplePromise) {
        var chainedPromise = this._chainedPromise;
        result.then(function (result) { chainedPromise.resolve(result); });
    } else
        this._chainedPromise.resolve(result);
}

function BenchmarkTestStep(testName, testFunction) {
    this.name = testName;
    this.run = testFunction;
}

var BenchmarkRunner = {_suites: [], _prepareReturnValue: null, _measuredValues: {}, _client: null};

BenchmarkRunner.suite = function (suite) {
    BenchmarkRunner._suites.push(suite);
}

BenchmarkRunner.setClient = function (client) {
    BenchmarkRunner._client = client;
}

BenchmarkRunner.waitForElement = function (selector) {
    var self = BenchmarkRunner;
    var promise = new SimplePromise;
    var contentDocument = self._frame.contentDocument;

    function resolveIfReady() {
        var element = contentDocument.querySelector(selector);
        if (element)
            return promise.resolve(element);
        setTimeout(resolveIfReady, 50);
    }

    resolveIfReady();
    return promise;
}

BenchmarkRunner._removeFrame = function () {
    var self = BenchmarkRunner;
    if (self._frame) {
        self._frame.parentNode.removeChild(self._frame);
        self._frame = null;
    }
}

BenchmarkRunner._appendFrame = function (src) {
    var self = BenchmarkRunner;
    var frame = document.createElement('iframe');
    frame.style.width = '800px';
    frame.style.height = '600px'
    document.body.appendChild(frame);
    self._frame = frame;
    return frame;
}

BenchmarkRunner._waitAndWarmUp = function () {
    var startTime = Date.now();

    function Fibonacci(n) {
        if (Date.now() - startTime > 100)
            return;
        if (n <= 0)
            return 0;
        else if (n == 1)
            return 1;
        return Fibonacci(n - 2) + Fibonacci(n - 1);
    }

    var promise = new SimplePromise;
    setTimeout(function () {
        Fibonacci(100);
        promise.resolve();
    }, 200);
    return promise;
}

// This function ought be as simple as possible. Don't even use SimplePromise.
BenchmarkRunner._runTest = function(suite, testFunction, prepareReturnValue, callback)
{
    var self = BenchmarkRunner;
    var now = window.performance && window.performance.now ? function () { return window.performance.now(); } : Date.now;

    var contentWindow = self._frame.contentWindow;
    var contentDocument = self._frame.contentDocument;

    var startTime = now();
    testFunction(prepareReturnValue, contentWindow, contentDocument);
    var endTime = now();
    var syncTime = endTime - startTime;

    var startTime = now();
    setTimeout(function () {
        var endTime = now();
        callback(syncTime, endTime - startTime);
    }, 0);
}

BenchmarkRunner._testName = function (suite, testName, metric) {
    if (!testName)
        return suite.name;
    return suite.name + '/' + testName + (metric ? '/' + metric : '');
}

function BenchmarkState(suites) {
    this._suites = suites;
    this._suiteIndex = -1;
    this._testIndex = 0;
    this.next();
}

BenchmarkState.prototype.currentSuite = function() {
    return this._suites[this._suiteIndex];
}

BenchmarkState.prototype.currentTest = function () {
    var suite = this.currentSuite();
    return suite ? suite.tests[this._testIndex] : null;
}

BenchmarkState.prototype.next = function () {
    this._testIndex++;

    var suite = this._suites[this._suiteIndex];
    if (suite && this._testIndex < suite.tests.length)
        return this;

    this._testIndex = 0;
    do {
        this._suiteIndex++;
    } while (this._suiteIndex < this._suites.length && this._suites[this._suiteIndex].disabled);

    return this;
}

BenchmarkState.prototype.isFirstTest = function () {
    return !this._testIndex;
}

BenchmarkState.prototype.prepareCurrentSuite = function (frame) {
    var self = this;
    var suite = this.currentSuite();
    var promise = new SimplePromise;
    frame.onload = function () {
        suite.prepare(frame.contentWindow, frame.contentDocument).then(function (result) { promise.resolve(result); });
    }
    frame.src = suite.url;
    return promise;
}

BenchmarkRunner.step = function (state) {
    var self = BenchmarkRunner;

    if (!state)
        state = new BenchmarkState(self._suites);

    var suite = state.currentSuite();
    if (!suite) {
        self._finalize();
        var promise = new SimplePromise;
        promise.resolve();
        return promise;
    }

    if (state.isFirstTest()) {
        self._masuredValuesForCurrentSuite = {};
        return state.prepareCurrentSuite(self._appendFrame()).then(function (prepareReturnValue) {
            self._prepareReturnValue = prepareReturnValue;
            return self._runTestAndRecordResults(state);
        });
    }

    return self._runTestAndRecordResults(state);
}

BenchmarkRunner._runTestAndRecordResults = function (state) {
    var self = BenchmarkRunner;
    var promise = new SimplePromise;
    var suite = state.currentSuite();
    var test = state.currentTest();

    if (self._client && self._client.willRunTest)
        self._client.willRunTest(suite, test);

    setTimeout(function () {
        self._runTest(suite, test.run, self._prepareReturnValue, function (syncTime, asyncTime) {
            var suiteResults = self._measuredValues[suite.name] || {tests:{}, total: 0};
            self._measuredValues[suite.name] = suiteResults;
            suiteResults.tests[test.name] = {'Sync': syncTime, 'Async': asyncTime};
            suiteResults.total += syncTime + asyncTime;

            if (self._client && self._client.willRunTest)
                self._client.didRunTest(suite, test);

            state.next();
            if (state.currentSuite() != suite)
                self._removeFrame();
            promise.resolve(state);
        });
    }, 0);
    return promise;
}

BenchmarkRunner._finalize = function () {
    var self = BenchmarkRunner;

    self._removeFrame();

    if (self._client && self._client.didRunSuites)
        self._client.didRunSuites(self._measuredValues);

    // FIXME: This should be done when we start running tests.
    self._measuredValues = {};
}
