// Benchmark for operationStringIncludes (multi-char search, no index)

function test(string, search) {
    return string.includes(search);
}
noInline(test);

// Create strings dynamically to avoid constant folding
function makeString(base) {
    return base + "";
}
noInline(makeString);

var string = makeString(".............................................okokHellookok................................");
var search1 = makeString("Hello");
var search2 = makeString("okHellooko");
var search3 = makeString("NotFound");

for (var i = 0; i < 1e6; ++i) {
    test(string, search1);
    test(string, search2);
    test(string, search3);
}
