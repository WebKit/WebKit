// Benchmark for String.prototype.endsWith with endPosition argument

function test(string, search, endPosition) {
    return string.endsWith(search, endPosition);
}
noInline(test);

function makeString(base) {
    return base + "";
}
noInline(makeString);

var string = makeString("................................okokHellookok.............................................");
var search1 = makeString("Hello");
var search2 = makeString("okok");

for (var i = 0; i < 1e6; ++i) {
    test(string, search1, string.length);
    test(string, search1, 41);
    test(string, search1, 40);
    test(string, search2, string.length);
    test(string, search2, 45);
}
