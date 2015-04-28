// This test branches on the boolean value of a string, which should be fast in the DFG.

function foo(string) {
    var bar;
    for (var i = 0; i < 10000000; ++i) {
        if (string)
            bar++;
    }
    return bar;
}

result = foo('hurr im a string');
