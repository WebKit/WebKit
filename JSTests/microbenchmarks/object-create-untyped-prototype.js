//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function test(prototype)
{
    return Object.create(prototype);
}
noInline(test);

var prototype1 = {};
var prototype2 = [];
for (var i = 0; i < 1e5; ++i) {
    test(prototype1);
    test(prototype2);
    test(null);
}
