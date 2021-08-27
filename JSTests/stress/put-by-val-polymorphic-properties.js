function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(object, name, value)
{
    object[name] = value;
}
noInline(test);

var array = [ 0, 1, 2 ];
array.hello = 42;
array.world = 44;

for (var i = 0; i < 1e6; ++i) {
    test(array, "hello", i);
    shouldBe(array.hello, i);
    test(array, "world", i);
    shouldBe(array.world, i);
    test(array, 0, i);
    shouldBe(array[0], i);
}
