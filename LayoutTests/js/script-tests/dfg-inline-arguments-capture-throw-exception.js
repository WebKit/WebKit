description(
"Tests what happens when you have an inlined function that captures arguments and then throws an exception."
);

var capturedArgs;

function foo(f, a, b, c) {
    var result = 0;
    capturedArgs = arguments;
    for (var i = 1; i < arguments.length; ++i)
        result += arguments[i] + f();
    return result + a + b + c;
}

var shouldThrow = false;

function bar(f) {
    return foo(f, 1, 2, 3);
}

function makeF(i) {
    return eval("(function() { if (shouldThrow) throw \"The exception " + i + "\"; return 0; })");
}

noInline(bar);
for (var i = 0; i < 100; i = dfgIncrement({f:bar, i:i + 1, n:100}))
    bar(makeF(i));

function recurse(n) {
    if (!n)
        return 42;
    return recurse(n - 1);
}

shouldThrow = true;

var theException;
try {
    bar(makeF(100));
    testFailed("bar() didn't throw an exception.");
} catch (e) {
    theException = e;
}

shouldBe("theException", "\"The exception 100\"");
recurse(1000);
shouldBe("capturedArgs.length", "4");
shouldBe("capturedArgs[1]", "1");
shouldBe("capturedArgs[2]", "2");
shouldBe("capturedArgs[3]", "3");
