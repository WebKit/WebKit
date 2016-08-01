function foo(x) {
    var tmp = x + 1;
    return tmp + arguments[0];
}

function bar(x) {
    return foo(x);
}

noInline(bar);

for (var i = 0; i < 10000; ++i) {
    var result = bar(i);
    if (result != i + i + 1)
        throw "Error: bad result: " + result;
}

var result = bar(4.5);
if (result != 4.5 + 4.5 + 1)
    throw "Error: bad result at end: " + result;
