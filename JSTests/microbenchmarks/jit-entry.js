//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
"use strict";

// run-jsc-benchmarks jitBoxEnabled:/Volumes/WebKit/ReleaseVersion/OpenSource/WebKitBuild/Release/jsc jitBoxDisabled:/Volumes/WebKit/ReleaseVersion/OpenSource/WebKitBuildBaseline/Release/jsc --microbenchmarks --benchmark=".*jit-entry.*" --inner 5 --outer 5

(function() {
    let result = 0;
    function test(a, b, c) {
        result += b
    }

    for (let i = 0; i < 1e6; ++i)
        $vm.callFromCPP(test.bind(null, 0, 1, 2), 100);

    if (result != 100 * 1e6)
        throw "Bad result: " + result;
})();

