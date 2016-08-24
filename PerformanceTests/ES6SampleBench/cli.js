const isInBrowser = false;

function makeBenchmarkRunner(sources, benchmarkConstructor) {
    let source = "'use strict';"
    for (let file of sources) {
        source += readFile(file);
    }
    source += `
        this.results = [];
        var benchmark = new ${benchmarkConstructor}();
        var numIterations = 200;
        for (var i = 0; i < numIterations; ++i) {
            var before = currentTime();
            benchmark.runIteration();
            var after = currentTime();
            results.push(after - before);
        }
    `;
    return function doRun() {
        let globalObjectOfScript = runString(source);
        let results = globalObjectOfScript.results;
        reportResult(results);
    }
}

load("driver.js");
load("results.js");
load("stats.js");
load("air_benchmark.js");
load("basic_benchmark.js");
load("glue.js");

driver.start(10);
