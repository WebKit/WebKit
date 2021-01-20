description("Regression test for https://webkit.org/b/150434.");

// This test verifies that we can process an exception thrown from a natively called function
// that was tail called from a JS function that was native called itself.
// We use bind to create a native wrapper around JS functions.

var myException = "This shouldn't crash!";

function bar(a, idx)
{
    "use strict";

    if (idx > 0)
        throw myException;

    return a;
}

boundBar = bar.bind(null, 42);

function foo(a, idx)
{
    "use strict";

    return boundBar(idx);
}

boundFoo = foo.bind(null, 41);

function test()
{
    for (var i = 0; i < 200000; i++) {
        try {
            if (boundFoo(i) != 42)
                testFailed("Got wrong result from foo()!");
        } catch (e) {
            if (e != myException)
                print(e);
        }
    }
}

noInline(test);

test();

testPassed("Properly handled an exception from a tail called native function that was called by a native function");
