//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function foo()
{
    for (var i = 0; i < 100000; ++i)
        new Error();
}

function bar()
{
    foo();
}

function baz()
{
    bar();
}

baz();
