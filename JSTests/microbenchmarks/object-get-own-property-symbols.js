//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
var object = {};
for (var i = 0; i < 20; ++i) {
    object[Symbol(i + 'prop')] = i;
}

function test(object)
{
    return Object.getOwnPropertySymbols(object);
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test(object);
