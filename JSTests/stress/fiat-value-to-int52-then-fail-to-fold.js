function foo() {
    return fiatInt52($vm.dfgTrue() ? 5.5 : 42) + 1;
}

noInline(foo);

for (var i = 0; i < testLoopCount; ++i) {
    var result = foo();
    if (result != 43 && result != 6.5)
        throw "Error: bad result: " + result;
}
