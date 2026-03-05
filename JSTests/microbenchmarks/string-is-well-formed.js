function testWellFormed(string) {
    return string.isWellFormed();
}
noInline(testWellFormed);

var wellFormedShort = "こんにちは世界";
var wellFormedMedium = "こんにちは世界".repeat(50);
var wellFormedLong = "こんにちは世界".repeat(500);

var withSurrogatePairs = "Hello 𠮷 World 𠮷".repeat(100);

for (var i = 0; i < 1e5; ++i) {
    testWellFormed(wellFormedLong);
    testWellFormed(withSurrogatePairs);
}
