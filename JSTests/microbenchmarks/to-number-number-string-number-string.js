//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function test(value)
{
    return +value;
}

var result = 0;
for (var i = 0; i < 1e4; ++i)
    result = test(i);

var result = 0;
for (var i = 0; i < 1e4; ++i)
    result = test(i.toString());

var result = 0;
for (var i = 0; i < 1e4; ++i)
    result = test(i);

var result = 0;
for (var i = 0; i < 1e4; ++i)
    result = test(i.toString());
