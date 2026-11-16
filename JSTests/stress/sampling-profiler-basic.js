if (platformSupportsSamplingProfiler()) {
    load("./sampling-profiler/samplingProfiler.js", "caller relative");

    function bar(y) {
        let x;
        for (let i = 0; i < 20; i++)
            x = new Error();
        return x;
    }
    noInline(bar);

    function foo() {
        bar(1000);
    }
    noInline(foo);

    function nothing(x) { return x; }
    noInline(nothing);

    runTest(foo, ["Error", "bar", "foo"]);

    function top() { 
        let x = 0;
        for (let i = 0; i < 25; i++) {
            x++;
            x--;
        }
    }

    function jaz(x) { return x + top(); }
    function kaz(y) {
        return jaz(y) + 5;
    }
    function checkInlining() {
        for (let i = 0; i < 100; i++)
            kaz(104);
    }

    // Tier it up.
    for (let i = 0; i < 1000; i++)
        checkInlining();

    runTest(checkInlining, ["jaz", "kaz", "checkInlining"]);
}
