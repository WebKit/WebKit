// Benchmark for String.prototype.startsWith (single-char search, no index)

function test(string, search) {
    return string.startsWith(search);
}
noInline(test);

function makeString(base) {
    return base + "";
}
noInline(makeString);

var string = makeString(".............................................okokHellookok................................");
var search1 = makeString(".");
var search2 = makeString("H");
var search3 = makeString("X");

for (var i = 0; i < 1e6; ++i) {
    test(string, search1);
    test(string, search2);
    test(string, search3);
}
