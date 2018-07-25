function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}
noInline(shouldBe);

function test1(array)
{
    for (var i = 0; i < 5; ++i) {
        array[0] = array[0] + 1;
    }
    return array;
}
noInline(test1);

function test2(array)
{
    for (var i = 0; i < 5; ++i) {
        array[0] = array[0] + 1;
    }
    return array;
}
noInline(test2);

function test3(array)
{
    for (var i = 0; i < 5; ++i) {
        array[0] = array[0] + 1;
    }
    return array;
}
noInline(test3);

for (var i = 0; i < 1e6; ++i) {
    shouldBe(String(test1([0, 1, 2, 3, 4])), `5,1,2,3,4`);
    shouldBe(String(test2([0.1, 1.1, 2.1, 3.1, 4.1])), `5.1,1.1,2.1,3.1,4.1`);
    shouldBe(String(test3(['C', 'o', 'c', 'o', 'a'])), `C11111,o,c,o,a`);
}
