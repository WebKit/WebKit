//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function test()
{
    return Object.create(null);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test();
