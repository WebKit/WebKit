import { BenchmarkRunner } from "./benchmark-runner.mjs";
import * as Statistics from "./statistics.mjs";
import { Suites } from "./tests.mjs";
import { renderMetricView } from "./metric-ui.mjs";
import { params } from "./params.mjs";
import { createDeveloperModeContainer } from "./developer-mode.mjs";

// FIXME(camillobruni): Add base class
class MainBenchmarkClient {
    developerMode = false;
    stepCount = null;
    suitesCount = null;
    _measuredValuesList = [];
    _finishedTestCount = 0;
    _progressCompleted = null;
    _isRunning = false;
    _hasResults = false;
    _developerModeContainer = null;
    _metrics = Object.create(null);

    constructor() {
        window.addEventListener("DOMContentLoaded", () => this.prepareUI());
        this._showSection(window.location.hash);
    }

    start() {
        if (this._startBenchmark())
            this._showSection("#running");
    }

    _startBenchmark() {
        if (this._isRunning)
            return false;

        if (Suites.every((suite) => suite.disabled)) {
            const message = `No suites selected - "${params.suites}" does not exist.`;
            alert(message);
            console.error(
                message,
                params.suites,
                "\nValid values:",
                Suites.map((each) => each.name)
            );

            return false;
        }

        this._developerModeContainer?.remove();
        this._progressCompleted = document.getElementById("progress-completed");
        if (params.iterationCount < 50) {
            const progressNode = document.getElementById("progress");
            for (let i = 1; i < params.iterationCount; i++) {
                const iterationMarker = progressNode.appendChild(document.createElement("div"));
                iterationMarker.className = "iteration-marker";
                iterationMarker.style.left = `${(i / params.iterationCount) * 100}%`;
            }
        }

        this._metrics = Object.create(null);
        this._isRunning = true;

        const enabledSuites = Suites.filter((suite) => !suite.disabled);
        const totalSubtestsCount = enabledSuites.reduce((testsCount, suite) => {
            return testsCount + suite.tests.length;
        }, 0);
        this.stepCount = params.iterationCount * totalSubtestsCount;
        this._progressCompleted.max = this.stepCount;
        this.suitesCount = enabledSuites.length;
        const runner = new BenchmarkRunner(Suites, this);
        runner.runMultipleIterations(params.iterationCount);
        return true;
    }

    get metrics() {
        return this._metrics;
    }

    willAddTestFrame(frame) {
        frame.style.left = "50%";
        frame.style.top = "50%";
        frame.style.transform = "translate(-50%, -50%)";
    }

    willRunTest(suite, test) {
        document.getElementById("info-label").textContent = suite.name;
        document.getElementById("info-progress").textContent = `${this._finishedTestCount} / ${this.stepCount}`;
    }

    didRunTest() {
        this._finishedTestCount++;
        this._progressCompleted.value = this._finishedTestCount;
    }

    didRunSuites(measuredValues) {
        this._measuredValuesList.push(measuredValues);
    }

    willStartFirstIteration() {
        this._measuredValuesList = [];
        this._finishedTestCount = 0;
    }

    didFinishLastIteration(metrics) {
        console.assert(this._isRunning);
        this._isRunning = false;
        this._hasResults = true;
        this._metrics = metrics;

        const scoreResults = this._computeResults(this._measuredValuesList, "score");
        this._updateGaugeNeedle(scoreResults.mean);
        document.getElementById("result-number").textContent = scoreResults.formattedMean;
        if (scoreResults.formattedDelta)
            document.getElementById("confidence-number").textContent = `\u00b1 ${scoreResults.formattedDelta}`;

        this._populateDetailedResults(metrics);

        if (params.developerMode)
            this.showResultsDetails();
        else
            this.showResultsSummary();
    }

    _computeResults(measuredValuesList, displayUnit) {
        function valueForUnit(measuredValues) {
            if (displayUnit === "ms")
                return measuredValues.geomean;
            return measuredValues.score;
        }

        function sigFigFromPercentDelta(percentDelta) {
            return Math.ceil(-Math.log(percentDelta) / Math.log(10)) + 3;
        }

        function toSigFigPrecision(number, sigFig) {
            const nonDecimalDigitCount = number < 1 ? 0 : Math.floor(Math.log(number) / Math.log(10)) + 1;
            return number.toPrecision(Math.max(nonDecimalDigitCount, Math.min(6, sigFig)));
        }

        const values = measuredValuesList.map(valueForUnit);
        const sum = values.reduce((a, b) => {
            return a + b;
        }, 0);
        const arithmeticMean = sum / values.length;
        let meanSigFig = 4;
        let formattedDelta;
        let formattedPercentDelta;
        const delta = Statistics.confidenceIntervalDelta(0.95, values.length, sum, Statistics.squareSum(values));
        if (!isNaN(delta)) {
            const percentDelta = (delta * 100) / arithmeticMean;
            meanSigFig = sigFigFromPercentDelta(percentDelta);
            formattedDelta = toSigFigPrecision(delta, 2);
            formattedPercentDelta = `${toSigFigPrecision(percentDelta, 2)}%`;
        }

        const formattedMean = toSigFigPrecision(arithmeticMean, Math.max(meanSigFig, 3));

        return {
            formattedValues: values.map((value) => {
                return `${toSigFigPrecision(value, 4)} ${displayUnit}`;
            }),
            mean: arithmeticMean,
            formattedMean: formattedMean,
            formattedDelta: formattedDelta,
            formattedMeanAndDelta: formattedMean + (formattedDelta ? ` \xb1 ${formattedDelta} (${formattedPercentDelta})` : ""),
        };
    }

    _addDetailedResultsRow(table, iterationNumber, value) {
        const row = document.createElement("tr");
        const th = document.createElement("th");
        th.textContent = `Iteration ${iterationNumber + 1}`;
        const td = document.createElement("td");
        td.textContent = value;
        row.appendChild(th);
        row.appendChild(td);
        table.appendChild(row);
    }

    _updateGaugeNeedle(score) {
        const needleAngle = Math.max(0, Math.min(score, 140)) - 70;
        const needleRotationValue = `rotate(${needleAngle}deg)`;

        const gaugeNeedleElement = document.querySelector("#summary > .gauge .needle");
        gaugeNeedleElement.style.setProperty("-webkit-transform", needleRotationValue);
        gaugeNeedleElement.style.setProperty("-moz-transform", needleRotationValue);
        gaugeNeedleElement.style.setProperty("-ms-transform", needleRotationValue);
        gaugeNeedleElement.style.setProperty("transform", needleRotationValue);
    }

    _populateDetailedResults(metrics) {
        const trackHeight = 24;
        document.documentElement.style.setProperty("--metrics-line-height", `${trackHeight}px`);
        const plotWidth = (params.viewport.width - 120) / 2;
        document.getElementById("geomean-chart").innerHTML = renderMetricView({
            metrics: [metrics.Geomean],
            width: plotWidth,
            trackHeight,
            renderChildren: false,
            colors: ["white"],
        });

        const toplevelMetrics = Object.values(metrics).filter((each) => !each.parent && each.children.length > 0);
        document.getElementById("tests-chart").innerHTML = renderMetricView({
            metrics: toplevelMetrics,
            width: plotWidth,
            trackHeight,
            renderChildren: false,
        });

        let html = "";
        for (const metric of toplevelMetrics) {
            html += renderMetricView({
                metrics: metric.children,
                width: plotWidth,
                trackHeight,
                title: metric.name,
            });
        }
        document.getElementById("metrics-results").innerHTML = html;

        const filePrefix = `speedometer-3-${new Date().toISOString()}`;
        let jsonData = this._formattedJSONResult({ modern: false });
        let jsonLink = document.getElementById("download-classic-json");
        jsonLink.href = URL.createObjectURL(new Blob([jsonData], { type: "application/json" }));
        jsonLink.setAttribute("download", `${filePrefix}.json`);

        jsonLink = document.getElementById("download-full-json");
        jsonData = this._formattedJSONResult({ modern: true });
        jsonLink.href = URL.createObjectURL(new Blob([jsonData], { type: "application/json" }));
        jsonLink.setAttribute("download", `${filePrefix}.json`);

        const csvData = this._formattedCSVResult();
        const csvLink = document.getElementById("download-csv");
        csvLink.href = URL.createObjectURL(new Blob([csvData], { type: "text/csv" }));
        csvLink.setAttribute("download", `${filePrefix}.csv`);
    }

    prepareUI() {
        window.addEventListener("hashchange", this._hashChangeHandler.bind(this));
        window.addEventListener("resize", this._resizeScreeHandler.bind(this));
        this._resizeScreeHandler();

        document.querySelectorAll("logo").forEach((button) => {
            button.onclick = this._logoClickHandler.bind(this);
        });
        document.getElementById("copy-full-json").onclick = this.copyJsonResults.bind(this);
        document.getElementById("copy-csv").onclick = this.copyCSVResults.bind(this);
        document.querySelectorAll(".start-tests-button").forEach((button) => {
            button.onclick = this._startBenchmarkHandler.bind(this);
        });

        if (params.suites.length > 0 || params.tags.length > 0)
            Suites.enable(params.suites, params.tags);

        if (params.developerMode) {
            this._developerModeContainer = createDeveloperModeContainer(Suites);
            document.body.append(this._developerModeContainer);
        }

        if (params.startAutomatically)
            this.start();
    }

    _hashChangeHandler() {
        this._showSection(window.location.hash);
    }

    _resizeScreeHandler() {
        // FIXME: Detect when the window size changes during the test.
        const mainSize = document.querySelector("main").getBoundingClientRect();
        const screenIsTooSmall = window.innerWidth < mainSize.width || window.innerHeight < mainSize.height;
        document.getElementById("min-screen-width").textContent = `${params.viewport.width + 50}px`;
        document.getElementById("min-screen-height").textContent = `${params.viewport.height + 50}px`;
        document.getElementById("screen-size").textContent = `${window.innerWidth}px by ${window.innerHeight}px`;
        document.getElementById("screen-size-warning").style.display = screenIsTooSmall ? null : "none";
    }

    _startBenchmarkHandler() {
        this.start();
    }

    _logoClickHandler(event) {
        // Prevent any accidental UI changes during benchmark runs.
        if (!this._isRunning)
            this._showSection("#home");
        event.preventDefault();
        return false;
    }

    showResultsSummary() {
        this._showSection("#summary");
    }

    showResultsDetails() {
        this._showSection("#details");
    }

    _formattedJSONResult({ modern = false }) {
        const indent = "    ";
        if (modern)
            return JSON.stringify(this._metrics, undefined, indent);
        return JSON.stringify(this._measuredValuesList, undefined, indent);
    }

    _formattedCSVResult() {
        // The CSV format is similar to the details view table. Each measurement is a row with
        // the name and N columns with the measurement for each iteration:
        // ```
        // Measurement,#1,...,#N
        // TodoMVC-JavaScript-ES5/Total,num,...,num
        // TodoMVC-JavaScript-ES5/Adding100Items,num,...,num
        // ...
        const labels = ["Name"];
        for (let i = 0; i < params.iterationCount; i++)
            labels.push(`#${i + 1}`);
        labels.push("Mean");
        const metrics = Array.from(Object.values(this._metrics)).filter((metric) => !metric.name.startsWith("Iteration-"));
        const metricsValues = metrics.map((metric) => [metric.name, ...metric.values, metric.mean].join(","));
        const csv = [labels.join(","), ...metricsValues];

        return csv.join("\n");
    }

    copyJsonResults() {
        navigator.clipboard.writeText(this._formattedJSONResult({ modern: true }));
    }

    copyCSVResults() {
        navigator.clipboard.writeText(this._formattedCSVResult());
    }

    _showSection(hash) {
        if (this._isRunning) {
            window.location.hash = "#running";
            return;
        } else if (this._hasResults) {
            if (hash !== "#summary" && hash !== "#details") {
                window.location.hash = "#summary";
                return;
            }
        } else {
            if (hash !== "#home" && hash !== "#about") {
                window.location.hash = "#home";
                return;
            }
        }
        window.location.hash = hash || "#home";
    }
}

const rootStyle = document.documentElement.style;
rootStyle.setProperty("--viewport-width", `${params.viewport.width}px`);
rootStyle.setProperty("--viewport-height", `${params.viewport.height}px`);

globalThis.benchmarkClient = new MainBenchmarkClient();
