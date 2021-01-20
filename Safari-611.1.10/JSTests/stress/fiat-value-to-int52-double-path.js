var array = new Float64Array(1);
array[0] = 42;

function foo() {
    return fiatInt52(bar()) + 1;
}

function bar() {
    return array[0];
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 1000000; ++i) {
    var result = foo();
    if (result != 43)
        throw "Error: bad result: " + result;
}
