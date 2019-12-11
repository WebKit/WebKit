//@ defaultNoSamplingProfilerRun

// Should not crash.
try {
    function foo(){
        [].slice({});
        foo();
    }
    foo();
} catch (e) {
}