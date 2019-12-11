//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function test()
{
    for (var i = 0; i < 10; ++i)
        var result = i.toString(10);
    return result;
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test();
