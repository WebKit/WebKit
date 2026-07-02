// Benchmark for String.prototype.startsWith with position argument

function test(string, search, index) {
    return string.startsWith(search, index);
}
noInline(test);

function makeString(base) {
    return base + "";
}
noInline(makeString);

var string = makeString(".............................................okokHellookok................................");
var search1 = makeString("Hello");
var search2 = makeString("okok");

for (var i = 0; i < 1e6; ++i) {
    test(string, search1, 0);
    test(string, search1, 49);
    test(string, search1, 50);
    test(string, search2, 0);
    test(string, search2, 45);
}
