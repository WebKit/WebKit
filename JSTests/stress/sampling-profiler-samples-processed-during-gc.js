//@ runDefault("--collectContinuously=1", "--sampleInterval=100", "--collectExtraSamplingProfilerData=1")

if (platformSupportsSamplingProfiler()) {
    startSamplingProfiler();

    function work() {
        let objects = [];
        for (let i = 0; i < 1000; ++i)
            objects.push({ i });
        return objects.length;
    }
    noInline(work);

    let startTime = Date.now();
    while (Date.now() - startTime < 1000)
        work();

    const noLine = 2 ** 32 - 1;
    let foundWorkLine = false;
    for (let trace of samplingProfilerStackTraces().traces) {
        for (let frame of trace.frames) {
            if (frame.name !== "work" || frame.line === noLine)
                continue;
            if (frame.line < 6 || frame.line > 11)
                throw new Error(`Bad line for work: ${JSON.stringify(frame)}`);
            foundWorkLine = true;
        }
    }
    if (!foundWorkLine)
        throw new Error("No samples in work with a line");
}
