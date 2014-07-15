function foo() {
    return fiatInt52(bar(DFGTrue())) + 1;
}

var thingy = false;
function bar(p) {
    if (thingy)
        return 5.5;
    return p ? 42 : 5.5;
}

noInline(foo);
noInline(bar);

for (var i = 0; i < 1000000; ++i) {
    var result = foo();
    if (result != 43 && result != 6.5)
        throw "Error: bad result: " + result;
}

thingy = true;
var result = foo();
if (result != 6.5)
    throw "Error: bad result at end: " + result;
