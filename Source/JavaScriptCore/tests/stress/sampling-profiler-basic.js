if (platformSupportsSamplingProfiler()) {
    load("./sampling-profiler/samplingProfiler.js");

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

    runTest(foo, ["<host>", "bar", "foo"]);

    function top() { 
        let x = 0;
        for (let i = 0; i < 25; i++) {
            x++;
            x--;
        }
    }
    noInline(top);
    // FIXME: We need to call top() to get the CallSiteIndex to update
    // inside the call frame. We need to fix that by having a PC=>CodeOrigin
    // mapping: https://bugs.webkit.org/show_bug.cgi?id=152629
    function jaz(x) { return x + top(); } // 
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
