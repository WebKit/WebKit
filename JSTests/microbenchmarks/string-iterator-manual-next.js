function sumCodePoints(string) {
    var result = 0;
    var iterator = string[Symbol.iterator]();
    var step;
    while (!(step = iterator.next()).done)
        result += step.value.codePointAt(0);
    return result;
}
noInline(sumCodePoints);

var string = "Hello, World! The quick brown fox jumps over the lazy dog.".repeat(4);
var expected = sumCodePoints(string);
for (var i = 0; i < 1e5; ++i) {
    var result = sumCodePoints(string);
    if (result !== expected)
        throw new Error("bad result: " + result);
}
