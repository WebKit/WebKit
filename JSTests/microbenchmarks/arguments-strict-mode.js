//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo() {
    "use strict";
    return [...arguments];

}

noInline(foo);

for (var i = 0; i < 200000; ++i) {
    var result = foo(i);
    if (result[0] != i)
        throw "Error: bad result: " + result;
}
