if (platformSupportsSamplingProfiler()) {
    load("./sampling-profiler/samplingProfiler.js", "caller relative");

    function foo() {
        let x;
        for (let i = 0; i < 1000; i++)
            x = new Error();
    }
    runTest(foo, ["Error", "foo"]);

    function bar() {
        let x;
        for (let i = 0; i < 1000; i++)
            x = new Function();
    }
    runTest(bar, ["Function", "bar"]);
}
