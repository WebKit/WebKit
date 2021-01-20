//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function target(func)
{
    return new func();
}
noInline(target);

for (var i = 0; i < 1e4; ++i)
    target(Array);
