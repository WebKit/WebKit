function foo() {
    return fiatInt52(bar(DFGTrue())) + 1;
}

function bar(p) {
    return p ? 42 : 5.5;
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 1000000; ++i) {
    var result = foo();
    if (result != 43 && result != 6.5)
        throw "Error: bad result: " + result;
}
