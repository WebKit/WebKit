function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

 function test(array)
{
    var iterator = array.values();
    return iterator.next().value;
}
noInline(test);

 var array = [1, 2, 3];
for (var i = 0; i < 1e6; ++i)
    shouldBe(test(array), 1);