description("Regression test for https://webkit.org/b/151279.");

// This test verifies that a megamorphic tail call from the first callee from C++ code
// works without crashing.

function bar() {
    return 11;
}

noInline(bar);

function foo(thisArgument)
{
    "use strict";

    return this.call(...arguments);
}

var fixedDate = new Date(2011, 11, 11, 11, 11, 11);
var boundFuncs = [];

boundFuncs[0] = foo.bind(Date.prototype.getSeconds, fixedDate);
boundFuncs[1] = foo.bind(Date.prototype.getMinutes, fixedDate);
boundFuncs[2] = foo.bind(Date.prototype.getHours, fixedDate);
boundFuncs[3] = foo.bind(Date.prototype.getDate, fixedDate);
boundFuncs[4] = foo.bind(Date.prototype.getMonth, fixedDate);
boundFuncs[5] = foo.bind(bar, 0);

function test()
{
    for (var i = 0; i < 200000; i++) {
        got = boundFuncs[i % 6]();
        if (got != 11)
            testFailed("Function returned " + got + " but expected 11!");
    }
}

noInline(test);

test();

testPassed("Properly handled megamorphic tail call from a JS entry function");
