//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(a) {
    return "foo" + a + "bar";
}

var result;
for (var i = 0; i < 1000000; ++i)
    result = foo("hello");

if (result != "foohellobar")
    throw "Error: bad result: " + result;
