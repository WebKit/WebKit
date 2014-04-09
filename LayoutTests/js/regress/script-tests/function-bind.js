function foo(a,b) {
    return a ^ b;
}

var g = foo.bind({});

var result = 0;
for (var i = 0; i < 100000; ++i) {
    result *= 3;
    result += g(i, result);
    result |= 0;
}

if (result != -2055604513)
    throw "Bad result: " + result;
