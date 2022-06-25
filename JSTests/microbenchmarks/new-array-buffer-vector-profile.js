//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function target()
{
    var array = [0];
    array.push(1);
    array.push(2);
    array.push(3);
    array.push(4);
    return array;
}
noInline(target);
for (var i = 0; i < 1e6; ++i)
    target();
