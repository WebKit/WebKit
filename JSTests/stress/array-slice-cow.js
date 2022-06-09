function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function testInt32()
{
    var array = [0, 1, 2, 3];
    return array.slice(1);
}
noInline(testInt32);

function testDouble()
{
    var array = [0.1, 1.1, 2.1, 3.1];
    return array.slice(1);
}
noInline(testDouble);

function testContiguous()
{
    var array = [true, false, true, false];
    return array.slice(1);
}
noInline(testContiguous);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(JSON.stringify(testInt32()), `[1,2,3]`);
    shouldBe(JSON.stringify(testDouble()), `[1.1,2.1,3.1]`);
    shouldBe(JSON.stringify(testContiguous()), `[false,true,false]`);
}
