//# sourceMappingURL=faster-than-light.js.map
//# sourceURL=faster-than-light.js

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual + " expected: " + expected);
}
(function () {
    if (!platformSupportsSamplingProfiler())
        return;

    load("./sampling-profiler/samplingProfiler.js", "caller relative");

    const timeToFail = 50000;
    let startTime = Date.now();
    do {
        let data = samplingProfilerStackTraces();
        for (let source of data.sources) {
            if (source.sourceURL && source.sourceMappingURL) {
                shouldBe(source.sourceURL, `faster-than-light.js`);
                shouldBe(source.sourceMappingURL, `faster-than-light.js.map`);
                return;
            }
        }
    } while (Date.now() - startTime < timeToFail);
    let stacktraces = samplingProfilerStackTraces();
    throw new Error(`Bad stack trace ${JSON.stringify(stacktraces)}`);
}());
