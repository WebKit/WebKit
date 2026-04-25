function test(string, search, index) {
    return string.includes(search, index);
}
noInline(test);

function makeString(base) {
    return base + "";
}
noInline(makeString);

var string = makeString(".............................................okokHellookok................................");
var searchH = makeString("H");
var searchX = makeString("X");
var searchDot = makeString(".");

for (var i = 0; i < 1e6; ++i) {
    test(string, searchH, 0);
    test(string, searchH, 49);
    test(string, searchH, 50);
    test(string, searchX, 0);
    test(string, searchDot, 57);
}
