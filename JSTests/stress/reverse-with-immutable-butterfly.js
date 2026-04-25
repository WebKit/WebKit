function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function reverseInt()
{
    var array = [0, 1, 2, 3];
    return array.reverse();
}

function reverseDouble()
{
    var array = [0.0, 1.1, 2.2, 3.3];
    return array.reverse();
}

function reverseContiguous()
{
    var array = [0.0, 1.1, 2.2, 'hello'];
    return array.reverse();
}

for (var i = 0; i < 1e4; ++i) {
    shouldBe(JSON.stringify(reverseInt()), `[3,2,1,0]`);
    shouldBe(JSON.stringify(reverseDouble()), `[3.3,2.2,1.1,0]`);
    shouldBe(JSON.stringify(reverseContiguous()), `["hello",2.2,1.1,0]`);
}
