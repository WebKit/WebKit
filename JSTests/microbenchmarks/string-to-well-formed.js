function testToWellFormed(string) {
    return string.toWellFormed();
}
noInline(testToWellFormed);

var wellFormedLong = "こんにちは世界".repeat(500);

var illFormedShort = "A\uD842B";
var illFormedMedium = ("Hello\uD842World").repeat(50);
var illFormedLong = ("Hello\uD842World").repeat(500);

var trailingLoneSurrogate = "こんにちは世界".repeat(100) + "\uD842";

for (var i = 0; i < 5e4; ++i) {
    testToWellFormed(wellFormedLong);
    testToWellFormed(illFormedLong);
    testToWellFormed(trailingLoneSurrogate);
}
