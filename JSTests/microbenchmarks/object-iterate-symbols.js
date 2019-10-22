//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
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
    test(obj);
