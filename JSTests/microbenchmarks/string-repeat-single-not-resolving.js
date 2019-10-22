//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function test(str, count)
{
    return str.repeat(count);
}

for (var i = 0; i < 1e4; ++i)
    test(' ', i);
