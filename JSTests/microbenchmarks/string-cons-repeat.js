//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(a) {
    return new String(a);
}

var result;
for (var i = 0; i < 1000000; ++i)
    result = foo("hello");

if (result != "hello")
    throw new "Error: bad result: " + result;
