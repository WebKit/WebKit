//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(a, b) {
    return arguments[0] + b;
}

noInline(foo);

for (var i = 0; i < 1000000; ++i) {
    var result = foo(i, 1);
    if (result != i + 1)
        throw "Error: bad result: " + result;
}
