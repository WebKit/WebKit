function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(array) {
    return "hello" + array + "ok";
}
noInline(test);

var array = [42];
for (var i = 0; i < 1e6; ++i)
    shouldBe(test(array), `hello42ok`);
