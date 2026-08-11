//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo() {
    return new Array();
}

function bar() {
    for (var i = 0; i < 10000000; ++i)
        foo();
}

bar();


