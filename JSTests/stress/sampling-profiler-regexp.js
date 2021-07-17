if (platformSupportsSamplingProfiler()) {
    load("./sampling-profiler/samplingProfiler.js");

    var text = `Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.`.repeat(1000);
    function getText()
    {
        return text;
    }
    noInline(getText);

    function test(regexp)
    {
        return getText().match(regexp);
    }
    noInline(test);

    function baz() {
        var regexp = /(.+)/gi;
        for (var i = 0; i < 1e1; ++i)
            test(regexp);
    }

    runTest(baz, "/(.+)/gi");
}
