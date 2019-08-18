function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [];
var casted = [];
for (var i = 0; i < 1e2; ++i) {
    array[i] = Math.random();
    casted[i] = 0;
}

function test(array, casted)
{
    for (var i = 0; i < array.length; i++)
        casted[i] = Number(array[i] < .5 ? 1 : 0);
}
noInline(test);

function check(array, casted)
{
    for (var i = 0; i < array.length; i++)
        shouldBe(casted[i], (array[i] < .5 ? 1 : 0));
}
noInline(check);

for (var i = 0; i < 1e5; ++i) {
    test(array, casted);
    check(array, casted);
}
