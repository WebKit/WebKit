//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
var object = {};
for (var i = 0; i < 1e3; ++i) {
    object[i + 'prop'] = i;
}

function test(object)
{
    return Object.keys(object).map(key => object[key]);
}
noInline(test);

for (var i = 0; i < 1e3; ++i)
    test(object);
