//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ $skipModes << :lockdown if $buildType == "debug"

var array = [];
var casted = [];
for (var i = 0; i < 1e3; ++i) {
    array[i] = Math.random();
    casted[i] = 0;
}

function test(array, casted)
{
    for (var i = 0; i < array.length; i++)
        casted[i] = Number(array[i] < .5);
}
noInline(test);

for (var i = 0; i < 1e5; ++i)
    test(array, casted);
