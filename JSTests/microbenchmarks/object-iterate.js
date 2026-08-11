//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
var obj = {
    a: 1,
    b: 2,
    c: 3,
    d: null,
    e: 'e'
};

function test(src) {
    var array = [];
    var keys = Object.keys(src);
    for (var i = 0; i < keys.length; ++i) {
        var key = keys[i];
        array.push(src[key]);
    }
    return array;
}
noInline(test);

for (var i = 0; i < 1e4; ++i)
    test(obj);
