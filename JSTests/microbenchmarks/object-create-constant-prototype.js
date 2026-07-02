//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function test(prototype)
{
    return Object.create(prototype);
}

var prototype = {foo: 42};
for (var i = 0; i < 1e5; ++i) {
    test(prototype);
}
