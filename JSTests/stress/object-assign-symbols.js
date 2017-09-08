function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

var a = Symbol("a");
var b = Symbol("b");
var c = Symbol("c");
var d = Symbol("d");
var e = Symbol("e");

var obj = {
    [a]: 1,
    [b]: 2,
    [c]: 3,
    [d]: null,
    [e]: 'e'
};

function test(src) {
    var o = {};
    var keys = Object.getOwnPropertySymbols(src);
    for (var i = 0; i < keys.length; ++i) {
        var key = keys[i];
        o[key] = src[key];
    }
    return o;
}
noInline(test);

for (var i = 0; i < 1e4; ++i) {
    var result = test(obj);
    shouldBe(result[a], 1);
    shouldBe(result[b], 2);
    shouldBe(result[c], 3);
    shouldBe(result[d], null);
    shouldBe(result[e], 'e');
}
