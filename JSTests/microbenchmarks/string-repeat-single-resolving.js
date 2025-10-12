//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function test(str, count)
{
    var repeated = str.repeat(count);
    // Expand the rope.
    return repeated[0] + repeated[count >> 1] + repeated[repeated.length - 1];
}

for (var i = 0; i < testLoopCount; ++i)
    test(' ', i);
