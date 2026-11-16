//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(a, inBounds) {
    a[0] = 1;
    if (!inBounds) {
        a[1] = 2;
        a[2] = 3;
    }
}

noInline(foo);

var array = new Int8Array(1);

for (var i = 0; i < 100000; ++i)
    foo(array, true);

for (var i = 0; i < 100000; ++i)
    foo(array, false);
