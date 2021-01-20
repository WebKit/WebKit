var index;

function foo() {
    if (index >= 0)
        return arguments[index];
    else
        return 13;
}

function bar() {
    return foo();
}

noInline(bar);

for (var i = 0; i < 100; ++i) {
    index = i & 1;
    var result = foo(42, 53);
    if (result != [42, 53][index])
        throw "Error: bad result in first loop: " + result;
}

for (var i = 0; i < 100000; ++i) {
    index = -(i & 1) - 1;
    var result = bar();
    if (result !== 13)
        throw "Error: bad result in second loop: " + result;
}

index = 0;
var result = bar();
if (result !== void 0)
    throw "Error: bad result at end: " + result;
