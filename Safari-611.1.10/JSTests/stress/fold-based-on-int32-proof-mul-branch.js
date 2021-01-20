function foo(a, b) {
    var value = $vm.dfgTrue() ? -0 : "foo";
    if (a * b == value)
        return [$vm.dfgTrue(), true];
    return [$vm.dfgTrue(), false];
}
noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo(1, 1);
    if (result[1] !== false)
        throw "Error: bad result: " + result;
}

var result = foo(-1, 0);
if (result[1] !== true && result[0])
    throw "Error: bad result at end: " + result;
