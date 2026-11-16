function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var array = [1, 2, 3, 4, 5];

function checking(i)
{
    if (i === (1e6 - 1)) {
        // array[0] = 42;
        array.ok = 4000;
    } else if (i === (2e6 - 4000)) {
        array.hey = 4000;
    } else if (i === (1e6 * 2)) {
        array[0] = 42;
    }
}
noInline(checking);

function test(i)
{
    checking(i);
    return array[0] + array[1];
}
noInline(test);

for (var i = 0; i < 2e6; ++i)
    shouldBe(test(i), 3);
shouldBe(test(2e6), 44);
