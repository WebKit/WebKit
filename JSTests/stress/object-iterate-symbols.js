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
    var array = [];
    var keys = Object.getOwnPropertySymbols(src);
    for (var i = 0; i < keys.length; ++i) {
        var key = keys[i];
        array.push(src[key]);
    }
    return array;
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    shouldBe(JSON.stringify(test(obj)), `[1,2,3,null,"e"]`);
