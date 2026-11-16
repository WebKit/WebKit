//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function thingy(a, b) {
    return a + b;
}

function doCall() {
    return thingy.apply(void 0, arguments);
}

function foo() {
    return doCall(1, 2);
}

for (var i = 0; i < 200000; ++i) {
    var result = foo();
    if (result != 3)
        throw "Bad result: " + result;
}

