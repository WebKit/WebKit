//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(a) {
    for (var i = 0; i < 100; ++i)
        a = new String(a);
    return a;
}

var result;
for (var i = 0; i < 10000; ++i)
    result = foo("hello");

if (result != "hello")
    throw new "Error: bad result: " + result;
