if (platformSupportsSamplingProfiler()) {
    load("./sampling-profiler/samplingProfiler.js", "caller relative");

    function foo(x) { 
        let o = {};
        for (let i = 0; i < 1000; i++) {
            let x = i;
            x--;
            o["x" + x] = x;
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
