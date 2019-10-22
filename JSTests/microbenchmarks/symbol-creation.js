//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function test()
{
    return Symbol();
}
noInline(test);

for (var i = 0; i < 4e5; ++i)
    test();
