//@ run("function-dot-arguments", "--useFunctionDotArguments=false")

function foo() {
    var a = bar.arguments;
    if (a.length != 0)
        throw "Error: arguments have non-zero length";
    for (var i = 0; i < 100; ++i) {
        if (a[i] !== void 0)
            throw "Error: argument " + i + " has non-undefined value";
    }
}

function bar() {
    foo();
}

bar();
bar(1);
bar(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);

