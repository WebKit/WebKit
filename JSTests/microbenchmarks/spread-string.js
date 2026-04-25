function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function test(string) {
    return [...string]
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    shouldBe(JSON.stringify(test('Hello')), `["H","e","l","l","o"]`);
    shouldBe(JSON.stringify(test('𠮷野家')), `["𠮷","野","家"]`);
}
