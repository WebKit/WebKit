if (platformSupportsSamplingProfiler()) {
    load("./sampling-profiler/samplingProfiler.js");

    function foo(x) { 
        for (let i = 0; i < 1000; i++) {
            let x = i;
            x--;
        }
        return x; 
    }
    noInline(foo);
    const limit = 300;
    let hellaDeep = function(i) {
        if (i < limit)
            hellaDeep(i + 1);
        else
            foo(i); 
    }

    let start = function() {
        hellaDeep(1);
    }

    let stackTrace = [];
    stackTrace.push("foo");
    for (let i = 0; i < limit; i++)
        stackTrace.push("hellaDeep");
    stackTrace.push("start");

    runTest(start, stackTrace);
}
