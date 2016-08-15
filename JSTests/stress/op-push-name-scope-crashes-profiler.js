//@ runProfiler
function test() {
    (function functionName() {
        ++counter;
        if (!arguments[0])
            return;
        eval("functionName(arguments[0] - 1, functionName, '' + functionName);");
     })(arguments[0]);
}

for (var i = 0; i < 1000; ++i) {
    counter = 0;
    test(100);
    if (counter !== 101) {
        throw "Oops, test(100) = " + test(100) + ", expected 101.";
    }
}
