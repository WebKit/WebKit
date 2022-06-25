description("Regression test for https://webkit.org/b/150336.");

// This test verifies that an OSR exit from a bound function with an inlined tail callee
// properly transitions to the baseline JIT without crashing.

myObj = {
    val: 1
}

function bar(a, idx)
{
    "use strict";

    if (idx == 9900)
        myObj.dfgOSR = "Test";

    if (idx == 199900)
        myObj.ftlOSR = "Test";

    return myObj.val + a;
}

function foo(a, idx)
{
    "use strict";

    return bar(a, idx);
}

boundFoo = foo.bind(null, 41);

function test()
{
    for (var i = 0; i < 200000; i++) {
        got = boundFoo(i);
        if (got != 42)
            testFailed("Function returned " + got + " but expected 42!");
    }
}

noInline(test);

test();

testPassed("Properly handled OSR exit from a bound function with an inlined tail callee.");
