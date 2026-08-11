//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo() {
    return new Array(4);
}

function bar() {
    for (var i = 0; i < 100000000; ++i)
        foo();
}

bar();
