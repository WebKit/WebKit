// Benchmark for String.prototype.endsWith (multi-char search, no endPosition)

function test(string, search) {
    return string.endsWith(search);
}
noInline(test);

// Create strings dynamically to avoid constant folding
function makeString(base) {
    return base + "";
}
noInline(makeString);

var string = makeString("................................okokHellookok.............................................");
var search1 = makeString(".....");
var search2 = makeString("Hello");
var search3 = makeString("NotFound");

for (var i = 0; i < 1e6; ++i) {
    test(string, search1);
    test(string, search2);
    test(string, search3);
}
