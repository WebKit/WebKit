function test(string, search) {
    return string.includes(search);
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
    test(string, searchH);
    test(string, searchX);
    test(string, searchDot);
}
