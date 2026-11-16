function countE(string) {
    var result = 0;
    for (var character of string) {
        if (character === 'e')
            result++;
    }
    return result;
}
noInline(countE);

var string = "Hello, World! The quick brown fox jumps over the lazy dog \u2014 twice.".repeat(4);
var expected = countE(string);
for (var i = 0; i < 1e5; ++i) {
    var result = countE(string);
    if (result !== expected)
        throw new Error("bad result: " + result);
}
