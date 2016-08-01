function foo() {
    return fiatInt52(DFGTrue() ? 5.5 : 42) + 1;
}

noInline(foo);

for (var i = 0; i < 1000000; ++i) {
    var result = foo();
    if (result != 43 && result != 6.5)
        throw "Error: bad result: " + result;
}
