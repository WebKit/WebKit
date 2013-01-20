function foo(a, b, c) {
    if (a + b < c)
        return a - b + c;
    else
        return a + b - c;
}

for (var i = 0; i < 1000000; ++i) {
    if (foo(1, 2, 3) != 0)
        throw new "Result not zero";
}

