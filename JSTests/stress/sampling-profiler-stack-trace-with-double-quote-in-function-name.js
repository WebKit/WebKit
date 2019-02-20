//@ runDefault("--useConcurrentJIT=false")
if (platformSupportsSamplingProfiler()) {
    function foo() {
        let obj = {};
        for (let i = 0; i < 10; ++i)
              obj[i + 'p'] = i;
    }
    noInline(foo);

    function test() {
        for (let i = 0; i < 1000; ++i) {
            foo();
            let stacktraces = samplingProfilerStackTraces();
            for (let stackTrace of stacktraces) { }
        }
    }

    startSamplingProfiler();
    foo.displayName = '"';
    test();
}
