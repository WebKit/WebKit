function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + String(actual) + ' ' + String(expected));
}

function test(v0, v1)
{
    var array = [v0, v1];
    for (var i = 0; i < 20; ++i)
        array.push(i);
    return array;
}
noInline(test);

for (var i = 0; i < 1e5; ++i) {
    var result = test(1, 2);
    shouldBe(JSON.stringify(result), `[1,2,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19]`);
}
