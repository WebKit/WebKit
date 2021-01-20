//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo(a, b) {
    return (a + b).length;
}

function bar() {
    return foo("hello ", "world!");
}

noInline(bar);

var result;
for (var i = 0; i < 1000000; ++i)
    result = bar();

if (result != "hello world!".length)
    throw "Error: bad result: " + result;
