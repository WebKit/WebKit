function test(v) {
    if (typeof v === 'number')
        return v | 0 === v;
    return v + 20;
}
noInline(test);

for (var i = 0; i < 1e6; ++i) {
    test(32);
    test("Hello");
}
