function test(str, len, fill) {
    var result = str.padStart(len, fill);
    return result.charCodeAt(0) + result.charCodeAt(50);
}
noInline(test);

for (var i = 0; i < 1e6; ++i)
    test("x", 100, "ab");
