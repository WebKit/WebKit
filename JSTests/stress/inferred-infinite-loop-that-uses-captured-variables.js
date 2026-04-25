function bar(f) {
    throw f;
}

noInline(bar);

var shouldContinue = true;

function foo(a) {
    var x = a + 1;
    while (shouldContinue) {
        bar(function() { return x; });
    }
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    try {
        foo(i);
    } catch (f) {
        var result = f();
        if (result != i + 1)
            throw "Error: bad result for i = " + i + ": " + result;
    }
}

