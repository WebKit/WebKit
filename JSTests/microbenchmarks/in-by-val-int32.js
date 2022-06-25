//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function test(object)
{
    if (1 in object)
        return object[1];
    return 0;
}
noInline(test);

var o1 = [42];
var o2 = [42, 41];

for (var i = 0; i < 1e6; ++i) {
    test(o1);
    test(o2);
}
