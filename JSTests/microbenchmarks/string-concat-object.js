//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(a) {
    return "foo" + new String(a) + "bar";
}

var result;
for (var i = 0; i < 100000; ++i)
    result = foo("hello");

if (result != "foohellobar")
    throw "Error: bad result: " + result;
