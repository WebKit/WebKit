
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

var BenchmarkRunner = {_suites: [], _prepareReturnValue: null, _measuredValues: {}};

BenchmarkRunner.suite = function (suite) {
    var self = BenchmarkRunner;
    self._suites.push(suite);
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

BenchmarkRunner._testItemId = function (suite, testName) {
    return suite.name + '-' + testName;
}

BenchmarkRunner.listSuites = function () {
    var self = BenchmarkRunner;

    var control = document.createElement('nav');

    var suites = self._suites;
    var ol = document.createElement('ol');
    var checkboxes = [];
    for (var suiteIndex = 0; suiteIndex < suites.length; suiteIndex++) {
        var suite = suites[suiteIndex];
        var li = document.createElement('li');
        var checkbox = document.createElement('input');
        checkbox.id = suite.name;
        checkbox.type = 'checkbox';
        checkbox.checked = true;
        checkboxes.push(checkbox);

        li.appendChild(checkbox);
        var label = document.createElement('label');
        label.appendChild(document.createTextNode(self._testName(suite)));
        li.appendChild(label);
        label.htmlFor = checkbox.id;

        var testList = document.createElement('ol');
        for (var testIndex = 0; testIndex < suite.tests.length; testIndex++) {
            var testItem = document.createElement('li');
            var test = suite.tests[testIndex];
            var anchor = document.createElement('a');
            anchor.id = self._testItemId(suite, test[0]);
            anchor.appendChild(document.createTextNode(self._testName(suite, test[0])));
            testItem.appendChild(anchor);
            testList.appendChild(testItem);
        }
        li.appendChild(testList);

        ol.appendChild(li);
    }

    control.appendChild(ol);

    var currentState = null;

    // Don't call step while step is already executing.
    var button = document.createElement('button');
    button.textContent = 'Step';
    button.onclick = function () {
        self.step(currentState).then(function (state) { currentState = state; });
    }
    control.appendChild(button);

    function callNextStep(state) {
        self.step(state).then(function (newState) {
            currentState = newState;
            if (newState)
                callNextStep(newState);
        });
    }

    var button = document.createElement('button');
    button.textContent = 'Run';
    button.onclick = function () { callNextStep(currentState); }
    control.appendChild(button);

    document.body.appendChild(control);
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
    } while (this._suiteIndex < this._suites.length && !document.getElementById(this._suites[this._suiteIndex].name).checked);

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

    var testName = test[0];
    var testItem = document.getElementById(self._testItemId(suite, testName));
    testItem.classList.add('running');
    setTimeout(function () {
        self._runTest(suite, test[1], self._prepareReturnValue, function (syncTime, asyncTime) {
            self._masuredValuesForCurrentSuite[self._testName(suite, testName, 'Sync')] = syncTime;
            self._masuredValuesForCurrentSuite[self._testName(suite, testName, 'Async')] = asyncTime;
            testItem.classList.remove('running');
            testItem.classList.add('ran');
            state.next();
            if (state.currentSuite() != suite) {
                var total = 0;
                for (var title in self._masuredValuesForCurrentSuite) {
                    var value = self._masuredValuesForCurrentSuite[title];
                    total += value;
                    self._measuredValues[title] = value;
                }
                self._measuredValues[self._testName(suite)] = total;
                self._removeFrame();
            }
            promise.resolve(state);
        });
    }, 0);
    return promise;
}

BenchmarkRunner._finalize = function () {
    var self = BenchmarkRunner;

    var results = '';
    var total = 0; // FIXME: Compute the total properly.
    for (var title in self._measuredValues) {
        results += title + ' : ' + self._measuredValues[title] + ' ms\n';
        total += self._measuredValues[title];
    }
    results += 'Total : ' + (total / 2) + ' ms\n';
    self._measuredValues = {};

    self._removeFrame();

    if (!results)
        return;

    var pre = document.createElement('pre');
    document.body.appendChild(pre);
    pre.textContent = results;
}

window.addEventListener('load', function () { BenchmarkRunner.listSuites(); });

(function () {
    var style = document.createElement('style');
    style.appendChild(document.createTextNode('iframe { width: 1000px; height: 500px; border: 2px solid black; }'
        + 'ol { list-style: none; margin: 0; padding: 0; }'
        + 'ol ol { margin-left: 2em; list-position: outside; }'
        + '.running { text-decoration: underline; }'
        + '.ran {color: grey}'
        + 'nav { position: absolute; right: 10px; }'));
    document.head.appendChild(style);
})();
