//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(a) {
    return a[1];
}

noInline(foo);

for (var i = 0; i < 100000; ++i) {
    var result = foo([42.5]);
    if (result !== void 0)
        throw "Error: bad value: " + result;
}
