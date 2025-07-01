function test(string) {
    return string.slice(-1);
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i) {
    test('Hello', 'o');
    test('', '');
    test('こんにちは', 'は');
}
