import { BenchmarkRunner } from "./benchmark-runner.mjs";
import { params } from "./params.mjs";
import { Suites } from "./tests.mjs";

class InteractiveBenchmarkRunner extends BenchmarkRunner {
    _stepPromise = undefined;
    _stepPromiseResolve = undefined;
    _isRunning = false;
    _isStepping = false;

    constructor(suites, iterationCount) {
        super(suites);
        this._client = this._createClient();
        if (!Number.isInteger(iterationCount) || iterationCount <= 0)
            throw Error("iterationCount must be a positive integer.");
        this._iterationCount = iterationCount;
    }

    _createClient() {
        return {
            willStartFirstIteration: this._start.bind(this),
            willRunTest: this._testStart.bind(this),
            didRunTest: this._testDone.bind(this),
            didRunSuites: this._iterationDone.bind(this),
            didFinishLastIteration: this._done.bind(this),
        };
    }

    _start() {
        if (this._isRunning)
            throw Error("Runner was not stopped before starting;");
        this._isRunning = true;
        if (this._isStepping)
            this._stepPromise = this._newStepPromise();
    }

    _step() {
        if (!this._stepPromise) {
            // Allow switching to stepping mid-run.
            this._stepPromise = this._newStepPromise();
        } else {
            const resolve = this._stepPromiseResolve;
            this._stepPromise = this._newStepPromise();
            resolve();
        }
    }

    _newStepPromise() {
        return new Promise((resolve) => {
            this._stepPromiseResolve = resolve;
        });
    }

    _testStart(suite, test) {
        test.anchor.classList.add("running");
    }

    async _testDone(suite, test) {
        const classList = test.anchor.classList;
        classList.remove("running");
        classList.add("ran");
        if (this._isStepping)
            await this._stepPromise;
    }

    _iterationDone(measuredValues) {
        let results = "";
        for (const suiteName in measuredValues.tests) {
            let suiteResults = measuredValues.tests[suiteName];
            for (const testName in suiteResults.tests) {
                let testResults = suiteResults.tests[testName];
                for (const subtestName in testResults.tests)
                    results += `${suiteName} : ${testName} : ${subtestName}: ${testResults.tests[subtestName]} ms\n`;
            }
            results += `${suiteName} : ${suiteResults.total} ms\n`;
        }
        results += `Arithmetic Mean : ${measuredValues.mean} ms\n`;
        results += `Geometric Mean : ${measuredValues.geomean} ms\n`;
        results += `Total : ${measuredValues.total} ms\n`;
        results += `Score : ${measuredValues.score} rpm\n`;

        if (!results)
            return;

        const pre = document.createElement("pre");
        document.body.appendChild(pre);
        pre.textContent = results;
    }

    _done() {
        this.isRunning = false;
    }

    runStep() {
        this._isStepping = true;
        if (!this._isRunning)
            this.runMultipleIterations(this._iterationCount);
        else
            this._step();
    }

    runSuites() {
        if (this._isRunning) {
            if (this._isStepping) {
                // Switch to continuous running only if we've been stepping.
                this._isStepping = false;
                this._step();
            }
        } else {
            this._isStepping = false;
            this.runMultipleIterations(this._iterationCount);
        }
    }
}

// Expose Suites/BenchmarkRunner for backwards compatibility
globalThis.BenchmarkRunner = InteractiveBenchmarkRunner;

function formatTestName(suiteName, testName) {
    return suiteName + (testName ? `/${testName}` : "");
}

function createUIForSuites(suites, onStep, onRunSuites) {
    const control = document.createElement("nav");
    const ol = document.createElement("ol");
    const checkboxes = [];
    for (let suiteIndex = 0; suiteIndex < suites.length; suiteIndex++) {
        const suite = suites[suiteIndex];
        const li = document.createElement("li");
        const checkbox = document.createElement("input");
        checkbox.id = suite.name;
        checkbox.type = "checkbox";
        checkbox.checked = !suite.disabled;
        checkbox.onchange = () => {
            suite.disabled = !checkbox.checked;
        };
        checkbox.onchange();
        checkboxes.push(checkbox);

        li.appendChild(checkbox);
        var label = document.createElement("label");
        label.appendChild(document.createTextNode(formatTestName(suite.name)));
        li.appendChild(label);
        label.htmlFor = checkbox.id;

        const testList = document.createElement("ol");
        for (let testIndex = 0; testIndex < suite.tests.length; testIndex++) {
            const testItem = document.createElement("li");
            const test = suite.tests[testIndex];
            const anchor = document.createElement("a");
            anchor.id = `${suite.name}-${test.name}`;
            test.anchor = anchor;
            anchor.appendChild(document.createTextNode(formatTestName(suite.name, test.name)));
            testItem.appendChild(anchor);
            testList.appendChild(testItem);
        }
        li.appendChild(testList);

        ol.appendChild(li);
    }

    control.appendChild(ol);

    let button = document.createElement("button");
    button.textContent = "Step";
    button.onclick = onStep;
    control.appendChild(button);

    button = document.createElement("button");
    button.textContent = "Run";
    button.id = "runSuites";
    button.onclick = onRunSuites;
    control.appendChild(button);

    button = document.createElement("button");
    button.textContent = "Select all";
    button.onclick = () => {
        for (var suiteIndex = 0; suiteIndex < suites.length; suiteIndex++) {
            suites[suiteIndex].disabled = false;
            checkboxes[suiteIndex].checked = true;
        }
    };
    control.appendChild(button);

    button = document.createElement("button");
    button.textContent = "Unselect all";
    button.onclick = () => {
        for (var suiteIndex = 0; suiteIndex < suites.length; suiteIndex++) {
            suites[suiteIndex].disabled = true;
            checkboxes[suiteIndex].checked = false;
        }
    };
    control.appendChild(button);

    return control;
}

function startTest() {
    if (params.suites.length > 0 || params.tags.length > 0)
        Suites.enable(params.suites, params.tags);

    const interactiveRunner = new window.BenchmarkRunner(Suites, params.iterationCount);
    if (!(interactiveRunner instanceof InteractiveBenchmarkRunner))
        throw Error("window.BenchmarkRunner must be a subclass of InteractiveBenchmarkRunner");

    // Don't call step while step is already executing.
    document.body.appendChild(createUIForSuites(Suites, interactiveRunner.runStep.bind(interactiveRunner), interactiveRunner.runSuites.bind(interactiveRunner)));

    if (params.startAutomatically)
        document.getElementById("runSuites").click();
}

window.addEventListener("load", startTest);
